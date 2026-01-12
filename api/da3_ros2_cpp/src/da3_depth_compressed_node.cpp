#include <chrono>
#include <cstring>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/qos.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace std::chrono_literals;

namespace {

size_t write_body_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  const size_t total = size * nmemb;
  auto *buf = reinterpret_cast<std::vector<uint8_t> *>(userdata);
  buf->insert(buf->end(), reinterpret_cast<uint8_t *>(ptr), reinterpret_cast<uint8_t *>(ptr) + total);
  return total;
}

static inline std::string trim(std::string s)
{
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
    s.pop_back();
  }
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
    ++i;
  }
  return s.substr(i);
}

static inline std::string to_lower(std::string s)
{
  for (auto &c : s) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return s;
}

struct DepthMeta
{
  int width = 0;
  int height = 0;
  std::string encoding;
  std::string endian;
  int is_metric = 0;
};

size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
  const size_t total = size * nitems;
  std::string line(buffer, buffer + total);

  auto *meta = reinterpret_cast<DepthMeta *>(userdata);

  const auto pos = line.find(':');
  if (pos == std::string::npos) {
    return total;
  }

  std::string key = to_lower(trim(line.substr(0, pos)));
  std::string val = trim(line.substr(pos + 1));

  if (key == "x-width") {
    meta->width = std::stoi(val);
  } else if (key == "x-height") {
    meta->height = std::stoi(val);
  } else if (key == "x-encoding") {
    meta->encoding = val;
  } else if (key == "x-endian") {
    meta->endian = val;
  } else if (key == "x-is-metric") {
    meta->is_metric = std::stoi(val);
  }

  return total;
}

struct UploadPayload
{
  std::vector<uint8_t> bytes;
  std::string mime_type;
  std::string filename;
};

static inline std::string join_infer_image_url(std::string backend_url)
{
  while (!backend_url.empty() && backend_url.back() == '/') {
    backend_url.pop_back();
  }
  const std::string suffix = "/infer_image";
  if (backend_url.size() >= suffix.size() &&
      backend_url.compare(backend_url.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return backend_url;
  }
  return backend_url + suffix;
}

}  // namespace

class Da3DepthCompressedNode : public rclcpp::Node
{
public:
  Da3DepthCompressedNode() : Node("da3_depth_compressed_node")
  {
    backend_url_ = this->declare_parameter<std::string>("backend_url", "http://127.0.0.1:8000");
    input_topic_ = this->declare_parameter<std::string>("input_topic", "/camera/image/compressed");
    output_topic_ = this->declare_parameter<std::string>("output_topic", "/camera/depth/image_raw");
    process_res_ = this->declare_parameter<int>("process_res", 504);
    process_res_method_ = this->declare_parameter<std::string>("process_res_method", "upper_bound_resize");
    align_to_input_ext_scale_ = this->declare_parameter<bool>("align_to_input_ext_scale", true);
    jpeg_quality_ = this->declare_parameter<int>("jpeg_quality", 90);
    timeout_ms_ = this->declare_parameter<int>("timeout_ms", 2000);

    depth_pub_ = this->create_publisher<sensor_msgs::msg::Image>(output_topic_, rclcpp::SensorDataQoS());

    sub_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
      input_topic_, rclcpp::SensorDataQoS(),
      std::bind(&Da3DepthCompressedNode::image_cb, this, std::placeholders::_1));

    curl_global_init(CURL_GLOBAL_DEFAULT);

    RCLCPP_INFO(get_logger(), "da3_depth_compressed_node listening on %s, publishing %s, backend=%s",
      input_topic_.c_str(), output_topic_.c_str(), backend_url_.c_str());
  }

  ~Da3DepthCompressedNode() override
  {
    curl_global_cleanup();
  }

private:
  void image_cb(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
  {
    auto payload = to_upload_payload(*msg);
    if (!payload) {
      return;
    }

    std::vector<uint8_t> depth_bytes;
    DepthMeta meta;

    if (!call_backend(*payload, depth_bytes, meta)) {
      return;
    }

    if (meta.encoding != "32FC1" || meta.width <= 0 || meta.height <= 0) {
      RCLCPP_WARN(get_logger(), "Unexpected backend response headers: encoding=%s w=%d h=%d",
        meta.encoding.c_str(), meta.width, meta.height);
      return;
    }

    const size_t expected = static_cast<size_t>(meta.width) * static_cast<size_t>(meta.height) * sizeof(float);
    if (depth_bytes.size() != expected) {
      RCLCPP_WARN(get_logger(), "Depth payload size mismatch: got=%zu expected=%zu", depth_bytes.size(), expected);
      return;
    }

    sensor_msgs::msg::Image out;
    out.header = msg->header;
    out.height = static_cast<uint32_t>(meta.height);
    out.width = static_cast<uint32_t>(meta.width);
    out.encoding = "32FC1";
    out.is_bigendian = 0;
    out.step = static_cast<sensor_msgs::msg::Image::_step_type>(meta.width * sizeof(float));
    out.data = std::move(depth_bytes);

    depth_pub_->publish(out);
  }

  std::optional<UploadPayload> to_upload_payload(const sensor_msgs::msg::CompressedImage &msg)
  {
    const auto fmt = to_lower(msg.format);

    if (msg.data.empty()) {
      RCLCPP_WARN(get_logger(), "Empty CompressedImage payload");
      return std::nullopt;
    }

    // If already JPEG/PNG, just forward bytes to the backend.
    if (fmt.find("jpeg") != std::string::npos || fmt.find("jpg") != std::string::npos) {
      UploadPayload p;
      p.bytes = msg.data;
      p.mime_type = "image/jpeg";
      p.filename = "frame.jpg";
      return p;
    }

    if (fmt.find("png") != std::string::npos) {
      UploadPayload p;
      p.bytes = msg.data;
      p.mime_type = "image/png";
      p.filename = "frame.png";
      return p;
    }

    // Fallback: decode then re-encode to JPEG.
    cv::Mat buf(1, static_cast<int>(msg.data.size()), CV_8UC1, const_cast<uint8_t *>(msg.data.data()));
    cv::Mat bgr = cv::imdecode(buf, cv::IMREAD_COLOR);
    if (bgr.empty()) {
      RCLCPP_WARN(get_logger(), "Failed to decode CompressedImage (format='%s')", msg.format.c_str());
      return std::nullopt;
    }

    std::vector<uint8_t> encoded;
    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(std::max(1, std::min(100, jpeg_quality_)));
    if (!cv::imencode(".jpg", bgr, encoded, params)) {
      RCLCPP_WARN(get_logger(), "Failed to JPEG-encode decoded CompressedImage");
      return std::nullopt;
    }

    UploadPayload p;
    p.bytes = std::move(encoded);
    p.mime_type = "image/jpeg";
    p.filename = "frame.jpg";
    return p;
  }

  bool call_backend(const UploadPayload &payload, std::vector<uint8_t> &depth_out, DepthMeta &meta)
  {
    CURL *curl = curl_easy_init();
    if (!curl) {
      RCLCPP_ERROR(get_logger(), "curl_easy_init failed");
      return false;
    }

    std::ostringstream url;
  url << join_infer_image_url(backend_url_)
        << "?process_res=" << process_res_
        << "&process_res_method=" << curl_easy_escape(curl, process_res_method_.c_str(), 0)
        << "&align_to_input_ext_scale=" << (align_to_input_ext_scale_ ? "true" : "false");

    curl_mime *form = curl_mime_init(curl);
    curl_mimepart *field = curl_mime_addpart(form);
    curl_mime_name(field, "image");
    curl_mime_filename(field, payload.filename.c_str());
    curl_mime_type(field, payload.mime_type.c_str());
    curl_mime_data(
      field, reinterpret_cast<const char *>(payload.bytes.data()), static_cast<size_t>(payload.bytes.size()));

    depth_out.clear();
    meta = DepthMeta{};

    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &depth_out);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &meta);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(std::max(1, timeout_ms_)));

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_mime_free(form);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      RCLCPP_WARN(get_logger(), "Backend request failed: %s", curl_easy_strerror(res));
      return false;
    }

    if (http_code != 200) {
      RCLCPP_WARN(get_logger(), "Backend HTTP %ld", http_code);
      return false;
    }

    return true;
  }

  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr sub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr depth_pub_;

  std::string backend_url_;
  std::string input_topic_;
  std::string output_topic_;
  int process_res_;
  std::string process_res_method_;
  bool align_to_input_ext_scale_;
  int jpeg_quality_;
  int timeout_ms_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Da3DepthCompressedNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}

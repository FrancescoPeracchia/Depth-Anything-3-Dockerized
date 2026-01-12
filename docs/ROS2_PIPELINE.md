# ROS2 Pipeline (DA3 backend → depth → visualization)

This repo provides two ROS2 Humble integration paths:

1) **Offline**: call the backend once, save `depth.raw`, then publish it as a ROS topic (`da3_ros2_py`).
2) **Streaming**: subscribe to a camera topic, call the backend per-frame, publish depth (`da3_ros2_cpp`).

---

## 0) Start the DA3 backend (Docker)

```bash
cd /home/francesco/Depth-Anything-3-Dockerized

docker run --rm -it --gpus all -p 8000:8000 \
  -e DA3_MODEL_DIR=depth-anything/DA3-SMALL \
  da3-backend:cu128
```

Sanity check:

```bash
curl -sS http://127.0.0.1:8000/status
```

---

## 1) Generate `depth.raw` from an image (PNG/JPG/JPEG all OK)

The endpoint is `POST /infer_image` and accepts multipart field **`image`** or **`file`**.

```bash
cd /home/francesco/Depth-Anything-3-Dockerized

curl -sS -D headers.txt \
  -X POST "http://127.0.0.1:8000/infer_image?process_res=512" \
  -F "image=@/home/francesco/test/images/rig1/camera1/image00300.jpg" \
  -o depth.raw
```

Read width/height (needed to publish the raw file as a ROS image):

```bash
grep -iE 'x-width|x-height' headers.txt
```

If you hit CUDA OOM, lower `process_res` (e.g. 384 or 256).

---

## 2) Offline ROS2: publish `depth.raw` and view it

### Build `da3_ros2_py`

```bash
python3 -m pip install -U colcon-ros colcon-cmake

source /opt/ros/humble/setup.bash
mkdir -p ~/ws_da3/src
ln -sf /home/francesco/Depth-Anything-3-Dockerized/api/da3_ros2_py ~/ws_da3/src/da3_ros2_py
cd ~/ws_da3
colcon build --packages-select da3_ros2_py
```

### Run publisher

Use the `X-Width` / `X-Height` values from `headers.txt`:

```bash
source /opt/ros/humble/setup.bash
source ~/ws_da3/install/setup.bash

ros2 run da3_ros2_py depth_raw_pub --ros-args \
  -p file_path:=/home/francesco/Depth-Anything-3-Dockerized/depth.raw \
  -p width:=252 \
  -p height:=140 \
  -p topic:=/camera/depth/image_raw \
  -p rate_hz:=5.0
```

### Verify + visualize

```bash
source /opt/ros/humble/setup.bash
source ~/ws_da3/install/setup.bash

ros2 topic info /camera/depth/image_raw -v
ros2 topic hz /camera/depth/image_raw

ros2 run rqt_image_view rqt_image_view
```

In `rqt_image_view`, select `/camera/depth/image_raw`.

---

## 3) Streaming ROS2: camera → backend → depth topic (`da3_ros2_cpp`)

The C++ node:
- subscribes (default) `/camera/image_raw`
- publishes (default) `/camera/depth/image_raw` (`32FC1`)

Run it (after building in your ROS2 workspace):

```bash
ros2 run da3_ros2_cpp da3_depth_node --ros-args \
  -p backend_url:=http://127.0.0.1:8000 \
  -p input_topic:=/camera/image_raw \
  -p output_topic:=/camera/depth/image_raw
```

### If your camera publishes `sensor_msgs/CompressedImage`

Use the compressed variant:

```bash
ros2 run da3_ros2_cpp da3_depth_compressed_node --ros-args \
  -p backend_url:=http://127.0.0.1:8000 \
  -p input_topic:=/camera/image/compressed \
  -p output_topic:=/camera/depth/image_raw
```

Then view `/camera/depth/image_raw` with `rqt_image_view` as above.

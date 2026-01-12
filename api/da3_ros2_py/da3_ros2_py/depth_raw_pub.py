import os
import sys
from array import array

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image


class DepthRawPublisher(Node):
    def __init__(self):
        super().__init__("da3_depth_raw_publisher")

        self.declare_parameter("file_path", "")
        self.declare_parameter("width", 0)
        self.declare_parameter("height", 0)
        self.declare_parameter("topic", "/camera/depth/image_raw")
        self.declare_parameter("frame_id", "camera")
        self.declare_parameter("rate_hz", 1.0)
        self.declare_parameter("oneshot", False)

        file_path = self.get_parameter("file_path").get_parameter_value().string_value
        width = int(self.get_parameter("width").get_parameter_value().integer_value)
        height = int(self.get_parameter("height").get_parameter_value().integer_value)
        topic = self.get_parameter("topic").get_parameter_value().string_value
        self._frame_id = self.get_parameter("frame_id").get_parameter_value().string_value
        rate_hz = float(self.get_parameter("rate_hz").get_parameter_value().double_value)
        self._oneshot = bool(self.get_parameter("oneshot").get_parameter_value().bool_value)

        if not file_path:
            raise RuntimeError("Parameter 'file_path' is required")
        if width <= 0 or height <= 0:
            raise RuntimeError("Parameters 'width' and 'height' must be > 0")
        if not os.path.exists(file_path):
            raise RuntimeError(f"depth.raw not found: {file_path}")

        expected_bytes = width * height * 4
        raw = self._read_exact(file_path)
        if len(raw) != expected_bytes:
            raise RuntimeError(
                f"Unexpected file size: {len(raw)} bytes (expected {expected_bytes} for {width}x{height} float32)"
            )

        floats = array("f")
        floats.frombytes(raw)
        if sys.byteorder != "little":
            floats.byteswap()

        msg = Image()
        msg.header.frame_id = self._frame_id
        msg.height = height
        msg.width = width
        msg.encoding = "32FC1"
        msg.is_bigendian = 0
        msg.step = width * 4
        msg.data = floats.tobytes()
        self._msg = msg

        self._pub = self.create_publisher(Image, topic, 10)

        if rate_hz <= 0.0:
            rate_hz = 1.0
        period_s = 1.0 / rate_hz
        self._timer = self.create_timer(period_s, self._tick)

        self.get_logger().info(
            f"Publishing {file_path} ({width}x{height}, 32FC1) on {topic} @ {rate_hz:.3f} Hz"
        )

    @staticmethod
    def _read_exact(path: str) -> bytes:
        with open(path, "rb") as f:
            return f.read()

    def _tick(self):
        self._msg.header.stamp = self.get_clock().now().to_msg()
        self._pub.publish(self._msg)
        if self._oneshot:
            rclpy.shutdown()


def main():
    rclpy.init()
    node = None
    try:
        node = DepthRawPublisher()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node is not None:
            node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()

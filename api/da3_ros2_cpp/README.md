# da3_ros2_cpp

ROS2 Humble C++ nodes that subscribe to camera topics and publish a depth `sensor_msgs/Image` (`32FC1`) by calling the DA3 backend endpoint `POST /infer_image`.

Executables:
- `da3_depth_node`: subscribes `sensor_msgs/Image`
- `da3_depth_compressed_node`: subscribes `sensor_msgs/CompressedImage`

## Topics
- `da3_depth_node` subscribes: `input_topic` (default: `/camera/image_raw`)
- `da3_depth_compressed_node` subscribes: `input_topic` (default: `/camera/image/compressed`)
- Both publish: `output_topic` (default: `/camera/depth/image_raw`)

## Parameters
- `backend_url` (default: `http://127.0.0.1:8000`)
- `process_res` (default: `504`)
- `process_res_method` (default: `upper_bound_resize`)
- `align_to_input_ext_scale` (default: `true`)
- `jpeg_quality` (default: `90`)

## Build
In a ROS2 workspace, symlink or copy this package into `src/`, then:

```bash
colcon build --packages-select da3_ros2_cpp
```

## Run

```bash
ros2 run da3_ros2_cpp da3_depth_node --ros-args \
  -p backend_url:=http://<backend-host>:8000 \
  -p input_topic:=/camera/image_raw \
  -p output_topic:=/camera/depth/image_raw
```

### Run (CompressedImage)

```bash
ros2 run da3_ros2_cpp da3_depth_compressed_node --ros-args \
  -p backend_url:=http://<backend-host>:8000 \
  -p input_topic:=/camera/image/compressed \
  -p output_topic:=/camera/depth/image_raw
```

# da3_ros2_py

Small ROS2 Humble Python utility to publish a `depth.raw` file (raw float32, little-endian, row-major) as `sensor_msgs/Image` with `encoding=32FC1`.

This is useful because `rqt_image_view` can only display ROS topics (it cannot open a `.raw` file directly).

## Build

In a ROS2 workspace:

Prerequisite: your `colcon` must have ROS extensions (not just `colcon-core`). If `colcon extensions` does not list `colcon-ros`, install it:

```bash
python3 -m pip install -U colcon-ros colcon-cmake
```

```bash
source /opt/ros/humble/setup.bash
mkdir -p ~/ws_da3/src
ln -sf /home/francesco/Depth-Anything-3-Dockerized/api/da3_ros2_py ~/ws_da3/src/da3_ros2_py
cd ~/ws_da3
colcon build --packages-select da3_ros2_py
source install/setup.bash
```

## Run

Example using the headers you got from the backend (see `headers.txt`):

```bash
source /opt/ros/humble/setup.bash
source ~/ws_da3/install/setup.bash

ros2 run da3_ros2_py depth_raw_pub --ros-args \
  -p file_path:=/home/francesco/Depth-Anything-3-Dockerized/depth.raw \
  -p width:=252 \
  -p height:=140 \
  -p topic:=/camera/depth/image_raw \
  -p frame_id:=camera \
  -p rate_hz:=5.0
```

If `ros2 run` still says `Package 'da3_ros2_py' not found` (usually because `colcon-ros` is missing), this workaround also works:

```bash
source /opt/ros/humble/setup.bash
source ~/ws_da3/install/setup.bash
export AMENT_PREFIX_PATH=~/ws_da3/install/da3_ros2_py:$AMENT_PREFIX_PATH
ros2 run da3_ros2_py depth_raw_pub --ros-args \
  -p file_path:=/home/francesco/Depth-Anything-3-Dockerized/depth.raw \
  -p width:=252 \
  -p height:=140
```

Then visualize:

```bash
ros2 run rqt_image_view rqt_image_view
```

Select `/camera/depth/image_raw` in the dropdown.

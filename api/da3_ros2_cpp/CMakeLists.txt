cmake_minimum_required(VERSION 3.8)
project(da3_ros2_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

add_executable(da3_depth_node src/da3_depth_node.cpp)

add_executable(da3_depth_compressed_node src/da3_depth_compressed_node.cpp)

target_include_directories(da3_depth_node PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${CURL_INCLUDE_DIRS}
)

target_include_directories(da3_depth_compressed_node PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${CURL_INCLUDE_DIRS}
)

target_link_libraries(da3_depth_node
  ${OpenCV_LIBRARIES}
  ${CURL_LIBRARIES}
)

target_link_libraries(da3_depth_compressed_node
  ${OpenCV_LIBRARIES}
  ${CURL_LIBRARIES}
)

target_compile_features(da3_depth_node PRIVATE cxx_std_17)

target_compile_features(da3_depth_compressed_node PRIVATE cxx_std_17)

target_compile_options(da3_depth_node PRIVATE ${CURL_CFLAGS_OTHER})

target_compile_options(da3_depth_compressed_node PRIVATE ${CURL_CFLAGS_OTHER})

ament_target_dependencies(da3_depth_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
)

ament_target_dependencies(da3_depth_compressed_node
  rclcpp
  sensor_msgs
  image_transport
  cv_bridge
)

install(TARGETS da3_depth_node da3_depth_compressed_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()

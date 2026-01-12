from setuptools import setup

package_name = "da3_ros2_py"

setup(
    name=package_name,
    version="0.0.1",
    packages=[package_name],
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="francesco",
    maintainer_email="francesco@example.com",
    description="ROS2 Humble Python node to publish DA3 depth.raw as sensor_msgs/Image (32FC1).",
    license="Apache-2.0",
    entry_points={
        "console_scripts": [
            "depth_raw_pub = da3_ros2_py.depth_raw_pub:main",
        ]
    },
)

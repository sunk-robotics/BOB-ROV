# Simple helper to launch ROV container for development
# Simply run ros2 launch bringup container.launch.py
# and then launch whatever container you want into it like:
# ros2 launch bringup camera.launch.py

from launch import LaunchDescription
from launch_ros.actions import ComposableNodeContainer

def generate_launch_description():
    return LaunchDescription([
        ComposableNodeContainer(
            name='rov_container',
            namespace='',
            package='rclcpp_components',
            executable='component_container',
            composable_node_descriptions=[],
            output='screen',
        )
    ])

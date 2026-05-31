from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import ComposableNodeContainer
from launch_ros.substitutions import FindPackageShare

def hardware_launch(filename, args={}):
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([FindPackageShare("bringup"), "launch", "hardware", filename])
        ]),
        launch_arguments=args.items()
    )

def generate_launch_description():
    container = ComposableNodeContainer(
        name='rov_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[],
        output='screen',
    )

    return LaunchDescription([
        container, 
        hardware_launch("imu.launch.py", {'container': 'rov_container'}),
        hardware_launch("temp_humid.launch.py", {'container': 'rov_container'}),
        hardware_launch("webrtc.launch.py", {'container': 'rov_container'})
    ])

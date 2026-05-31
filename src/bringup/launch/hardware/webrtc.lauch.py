from launch import LaunchDescription, descriptions
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import LoadComposableNodes
from launch_ros.descriptions import ComposableNode
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    container_arg = DeclareLaunchArgument(
        'container',
        default_value='rov_container',
        description="Name of the component container to load into"
    )

    config = PathJoinSubstitution([
        FindPackageShare('bringup'),
        'config', 'hardware', 'webrtc.yaml'
    ])

    load_webrtc = LoadComposableNodes(
        target_container=LaunchConfiguration('container'),
        composable_node_descriptions=[
            ComposableNode(
                package='comms',
                plugin='comms::WebRtcNode',
                name='webrtc_node',
                parameters=[config]
            )
        ]
    )

    return LaunchDescription([container_arg, load_webrtc])

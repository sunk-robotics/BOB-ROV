cmake_minimum_required(VERSION 3.8)
project(sensors)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# find dependencies
find_package(ament_cmake REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(i2c_utils REQUIRED)
find_package(bno055_vendor REQUIRED)

add_library(imu_node SHARED src/imu_node.cpp)
add_library(temp_humid_node SHARED src/temp_humid_node.cpp)

target_include_directories(imu_node PUBLIC include)
target_include_directories(temp_humid_node PUBLIC include)

target_link_libraries(imu_node bno055_vendor::bno055 i2c_utils::i2c_utils)
target_link_libraries(temp_humid_node i2c_utils::i2c_utils)

ament_target_dependencies(imu_node rclcpp rclcpp_components sensor_msgs)
ament_target_dependencies(temp_humid_node rclcpp rclcpp_components sensor_msgs)

rclcpp_components_register_node(
  imu_node
  PLUGIN "sensors::ImuNode"
  EXECUTABLE imu_node_exe
)
rclcpp_components_register_node(
  temp_humid_node
  PLUGIN "sensors::TempHumidSensorNode"
  EXECUTABLE temp_humid_exe
)

install(TARGETS
  imu_node
  temp_humid_node
  DESTINATION lib/
)

install(TARGETS
  imu_node_exe
  temp_humid_exe
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()

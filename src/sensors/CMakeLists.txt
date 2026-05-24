cmake_minimum_required(VERSION 3.8)
project(sensors)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(i2c_utils REQUIRED)

add_library(imu_node SHARED src/imu_node.cpp)
add_library(BNO055_SensorAPI STATIC
  vendor/BNO055_SensorAPI/bno055.c
)
set_target_properties(BNO055_SensorAPI PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
)
target_include_directories(BNO055_SensorAPI PUBLIC
  vendor/BNO055_SensorAPI
)

target_include_directories(imu_node PUBLIC
  include
)

target_link_libraries(imu_node BNO055_SensorAPI i2c_utils::i2c_utils)

rclcpp_components_register_node(
  imu_node
  PLUGIN "ImuNode"
  EXECUTABLE imu_node_exe
)

ament_target_dependencies(imu_node rclcpp rclcpp_components sensor_msgs)


install(TARGETS
  imu_node
  imu_node_exe
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

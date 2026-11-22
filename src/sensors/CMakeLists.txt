cmake_minimum_required(VERSION 3.20)
project(sensors)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(i2c REQUIRED)
find_package(bno055_vendor REQUIRED)

# ---- Header-only driver libraries ----

function(add_sensors_interface_library target_name)
  add_library(${target_name} INTERFACE)
  target_include_directories(${target_name} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
  target_compile_features(${target_name} INTERFACE cxx_std_23)
endfunction()

add_sensors_interface_library(sensors_checksum)
add_sensors_interface_library(sensors_aht20)
add_sensors_interface_library(sensors_bno055)
add_sensors_interface_library(sensors_ld)

target_link_libraries(sensors_aht20  INTERFACE i2c::i2c sensors_checksum)
target_link_libraries(sensors_bno055 INTERFACE i2c::i2c bno055_vendor::bno055)
target_link_libraries(sensors_ld     INTERFACE i2c::i2c)

# ---- Nodes ----

add_library(imu_node SHARED src/imu_node.cpp)
target_link_libraries(imu_node PRIVATE
  sensors_bno055
  rclcpp::rclcpp
  rclcpp_components::component
  ${sensor_msgs_TARGETS})

target_link_libraries(imu_node PRIVATE sensors_bno055)

add_library(temp_humid_node SHARED src/temp_humid_node.cpp)
target_link_libraries(temp_humid_node PRIVATE
  sensors_aht20
  rclcpp::rclcpp
  rclcpp_components::component
  ${sensor_msgs_TARGETS})

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

# ---- Install / export ----

install(DIRECTORY include/ DESTINATION include)

install(TARGETS sensors_checksum sensors_aht20 sensors_bno055 sensors_ld
  EXPORT export_${PROJECT_NAME})

install(TARGETS imu_node temp_humid_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS imu_node_exe temp_humid_exe
  DESTINATION lib/${PROJECT_NAME})

ament_export_targets(export_${PROJECT_NAME})
ament_export_dependencies(i2c bno055_vendor)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()

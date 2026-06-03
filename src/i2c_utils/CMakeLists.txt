cmake_minimum_required(VERSION 3.8)
project(i2c_utils)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

add_library(i2c_utils INTERFACE)

target_include_directories(i2c_utils INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

option(I2C_UTILS_ROS_LOGGING "Use rclcpp logging" OFF)

if(I2C_UTILS_ROS_LOGGING)
  find_package(rclcpp REQUIRED)
  target_link_libraries(i2c_utils INTERFACE rclcpp::rclcpp)
  target_compile_definitions(i2c_utils INTERFACE I2C_UTILS_ROS_LOGGING)
  ament_export_dependencies(rclcpp)
endif()

install(DIRECTORY include/ DESTINATION include)
install(TARGETS i2c_utils EXPORT i2c_utils_targets)

ament_export_targets(i2c_utils_targets HAS_LIBRARY_TARGET)
# ament_export_include_directories(include)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  # set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  # set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  add_executable(test_i2c_bus test/test_i2c_bus.cpp)
  target_link_libraries(test_i2c_bus PRIVATE i2c_utils)
endif()

ament_package()

cmake_minimum_required(VERSION 3.8)
project(control)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)

find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(spi_utils REQUIRED)

add_library(motor_controller_node SHARED src/motor_controller_node.cpp)

target_include_directories(motor_controller_node PUBLIC include)

target_link_libraries(motor_controller_node spi_utils::spi_utils)

ament_target_dependencies(motor_controller_node rclcpp rclcpp_components)

rclcpp_components_register_node(
  motor_controller_node
  PLUGIN "control::MotorControllerNode"
  EXECUTABLE motor_controller_node_exe
)

install(TARGETS
  motor_controller_node
  DESTINATION lib/
)

install(TARGETS
  motor_controller_node_exe
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

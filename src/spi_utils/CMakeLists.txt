cmake_minimum_required(VERSION 3.8)
project(spi_utils)

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

add_library(spi_utils INTERFACE)

target_include_directories(spi_utils INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

option(SPI_UTILS_ROS_LOGGING "Use rlcpp logging" OFF)

if(SPI_UTILS_ROS_LOGGING)
  find_package(rclcpp REQUIRED)
  target_link_libraries(spi_utils INTERFACE rclcpp::rclcpp)
  target_compile_definitions(spi_utils INTERFACE SPI_UTILS_ROS_LOGGING)
  ament_export_dependencies(rclcpp)
endif()

install(DIRECTORY include/
  DESTINATION include
)

install(TARGETS spi_utils
  EXPORT spi_utils_targets
)

ament_export_targets(spi_utils_targets HAS_LIBRARY_TARGET)

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

  add_executable(test_spi_bus test/test_spi_bus.cpp)
  target_link_libraries(test_spi_bus PRIVATE spi_utils)
endif()

ament_package()

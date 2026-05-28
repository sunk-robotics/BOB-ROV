cmake_minimum_required(VERSION 3.8)
project(comms)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(libdatachannel_vendor REQUIRED)
find_package(LibDataChannel REQUIRED)

add_library(webrtc_node SHARED src/webrtc_node.cpp)

target_include_directories(webrtc_node PUBLIC include)
target_include_directories(webrtc_node PRIVATE external/httplib)
target_compile_definitions(webrtc_node PRIVATE
  CPPHTTPLIB_THREAD_POOL_COUNT=1
  CPPHTTPLIB_THREAD_POOL_MAX_COUNT=1
)

target_link_libraries(webrtc_node LibDataChannel::LibDataChannel)

ament_target_dependencies(webrtc_node rclcpp rclcpp_components)

rclcpp_components_register_node(
  webrtc_node
  PLUGIN "comms::webrtc_node"
  EXECUTABLE webrtc_node_exe
)

install(TARGETS webrtc_node DESTINATION lib/)
install(TARGETS webrtc_node_exe DESTINATION lib/${PROJECT_NAME})

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

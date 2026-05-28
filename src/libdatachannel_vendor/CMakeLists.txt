cmake_minimum_required(VERSION 3.10)
project(libdatachannel_vendor)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_vendor_package REQUIRED)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
find_package(LibDataChannel QUIET)

ament_vendor(libdatachannel_vendor
  SATISFIED ${LibDataChannel_FOUND}
  VCS_TYPE git
  VCS_URL https://github.com/paullouisageneau/libdatachannel.git
  VCS_VERSION v0.24.3
  CMAKE_ARGS
    -DUSE_GNUTLS=0
    -DUSE_NICE=0
    -DCMAKE_BUILD_TYPE=Release
)

install(DIRECTORY cmake DESTINATION share/${PROJECT_NAME})

ament_package()

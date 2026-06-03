cmake_minimum_required(VERSION 3.10)
project(libdatachannel_vendor)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_vendor_package REQUIRED)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
find_package(LibDataChannel QUIET)

ament_vendor(libdatachannel_vendor
  #  SATISFIED ${LibDataChannel_FOUND}
  SATISFIED FALSE # false until merge my changes regarding not silently dropping an error in Track::sendFrame()
  VCS_TYPE git
  VCS_URL https://github.com/Rtificer/libdatachannel.git
  VCS_VERSION 271c1edb874c8d5b34f8f55129e93cb3f83a3a95
  CMAKE_ARGS
    -DUSE_GNUTLS=0
    -DUSE_NICE=0
    -DCMAKE_BUILD_TYPE=Release
)

install(DIRECTORY cmake DESTINATION share/${PROJECT_NAME})

ament_package()

#!/bin/bash
# setup.sh

# System dependencies for libcamera Pi fork
sudo apt install -y \
  python3-pip git \
  python3-jinja2 python3-yaml python3-ply \
  libboost-dev \
  libgnutls28-dev openssl libtiff-dev pybind11-dev \
  meson cmake python3-colcon-meson \
  libglib2.0-dev libgstreamer-plugins-base1.0-dev \
  libboost-log-dev libboost-thread-dev

source /opt/ros/$ROS_DISTRO/setup.bash

# Import source dependencies
vcs import src/ <dependencies.repos

# Install binary dependencies
# --skip-keys=libcamera: camera_ros depends on libcamera but we build
# the Raspberry Pi fork from source (src/libcamera) for HQ camera support.
# Installing the system binary would conflict with our source build.
rosdep install -y --from-paths src --ignore-src \
  --rosdistro $ROS_DISTRO \
  --skip-keys=libcamera

colcon build --symlink-install --event-handlers=console_direct+

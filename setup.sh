#!/bin/bash
# setup.sh

set -e # stop on any error

# Configure apt sources — required for ROS2 binary install on Ubuntu Pi
# (Pi Ubuntu images don't include noble-updates/backports by default)
if ! grep -q "noble-updates noble-backports" /etc/apt/sources.list.d/ubuntu.sources; then
  sudo sed -i 's/Suites: noble/Suites: noble noble-updates noble-backports/' \
    /etc/apt/sources.list.d/ubuntu.sources
fi

# Add universe repo and install curl before anything else
sudo apt update
sudo apt install -y software-properties-common curl
sudo add-apt-repository -y universe

# Add ROS2 apt source
export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F'"' '{print $4}')
curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo ${UBUNTU_CODENAME:-${VERSION_CODENAME}})_all.deb"
sudo dpkg -i /tmp/ros2-apt-source.deb

# Install ROS2 — ros-base instead of desktop (no GUI tools needed on headless Pi)
# ros-dev-tools includes colcon, rosdep, vcstool and other build tools
sudo apt update && sudo apt upgrade -y
sudo apt install -y ros-jazzy-ros-base ros-dev-tools

# Initialize rosdep (|| true so it doesn't fail if already initialized)
sudo rosdep init || true
rosdep update

# Source ROS2 for the rest of this script
source /opt/ros/jazzy/setup.bash

# libcamera Pi fork build dependencies
# (not handled by rosdep as libcamera is a meson project outside the ROS ecosystem)
sudo apt install -y \
  python3-jinja2 python3-yaml python3-ply \
  libboost-dev libboost-log-dev libboost-thread-dev \
  libgnutls28-dev openssl libtiff-dev pybind11-dev \
  meson python3-colcon-meson \
  libglib2.0-dev libgstreamer-plugins-base1.0-dev \
  libudev-dev libyaml-dev

# Import source dependencies (libcamera Pi fork, camera_ros, bno055)
vcs import src/ <dependencies.repos

# Install all remaining binary ROS dependencies via rosdep
# --skip-keys=libcamera: camera_ros declares libcamera as a dependency but
# we build the Raspberry Pi fork from source for HQ camera module support.
# The system binary package would conflict with our source build.
rosdep install -y --from-paths src --ignore-src \
  --rosdistro $ROS_DISTRO \
  --skip-keys=libcamera

colcon build --symlink-install --event-handlers=console_direct+

# Add ROS2 source to bashrc if not already there
if ! grep -q "source /opt/ros/jazzy/setup.bash" ~/.bashrc; then
  echo "source /opt/ros/jazzy/setup.bash" >>~/.bashrc
fi

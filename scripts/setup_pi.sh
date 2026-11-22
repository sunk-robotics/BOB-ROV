#!/bin/bash
# setup_pi.sh

set -e # stop on any error

: "${UBUNTU_PRO_TOKEN:?UBUNTU_PRO_TOKEN must be set (passed in by setup.sh)}"

PI_WS="/home/sunk/ros2_ws"
mkdir -p "$PI_WS"
cd "$PI_WS"

# Remove automatic background updates so nothing interrupts realtime work
# or holds the apt/dpkg lock during setup
sudo systemctl disable --now cron

TIMERS_TO_MASK=(
  apt-daily.timer
  apt-daily-upgrade.timer
  sysstat-collect.timer
  sysstat-summary.timer
  fwupd-refresh.timer
  motd-news.timer
  update-notifier-motd.timer
  update-notifier-download.timer
)
sudo systemctl mask --now "${TIMERS_TO_MASK[@]}"
sudo apt-get remove -y unattended-upgrades

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

# Enable SPI and I2C kernel modules
if ! grep -q "dtparam=spi=on" /boot/firmware/config.txt; then
  echo "dtparam=spi=on" | sudo tee -a /boot/firmware/config.txt
fi
if ! grep -q "dtparam=i2c_arm=on" /boot/firmware/config.txt; then
  echo "dtparam=i2c_arm=on" | sudo tee -a /boot/firmware/config.txt
fi

# Create the realtime group (--force: no error if it already exists)
sudo groupadd --force realtime

# Add user to nessesary groups
sudo usermod -aG gpio,i2c,spi,dialout,video,realtime sunk

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

# Remove empty subproject dirs that block Meson's wrap fallback
# don't know why but sometimes this is an issue
rm -rf src/libcamera/subprojects/libpisp

# Initialize submodules
git -C src/libcamera submodule update --init --recursive

# Install all remaining binary ROS dependencies via rosdep
# --skip-keys=libcamera: camera_ros declares libcamera as a dependency but
# we build the Raspberry Pi fork from source for HQ camera module support.
# The system binary package would conflict with our source build.
rosdep install -y --from-paths src --ignore-src \
  --rosdistro $ROS_DISTRO \
  --skip-keys=libcamera

# Add ROS2 source to bashrc if not already there
if ! grep -q "source /opt/ros/jazzy/setup.bash" ~/.bashrc; then
  echo "source /opt/ros/jazzy/setup.bash" >>~/.bashrc
fi

# Realtime kernel (Ubuntu Pro). Takes effect on the reboot below.
if ! sudo pro status --format json | grep -q '"attached": true'; then
  sudo pro attach "$UBUNTU_PRO_TOKEN"
fi
sudo pro enable realtime-kernel --variant=raspi --assume-yes

sudo reboot || true

#!/bin/bash
# deploy.sh

set -e # exit on any error

PI="bob-rov-rp5"
PI_WS="/home/sunk/ros2_ws"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Syncing files..."
rsync -avz --delete \
  --exclude build \
  --exclude install \
  --exclude log \
  --exclude .git \
  --exclude .cache \
  "$SCRIPT_DIR/" "$PI:$PI_WS/"

echo "==> Building on Pi..."

ssh -t "$PI" "
    rm -rf $PI_WS/src/libcamera/subprojects/libpisp &&
    source /opt/ros/jazzy/setup.bash &&
    cd $PI_WS &&
    colcon build --symlink-install
"

echo "==> Done!"

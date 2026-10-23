#!/bin/bash
# update_config.sh

set -e

PI="bob-rov-rp5"
PI_WS="/home/sunk/ros2_ws"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rsync -avz --delete \
  "$SCRIPT_DIR/src/bringup/config/" \
  "$PI:$PI_WS/src/bringup/config/"

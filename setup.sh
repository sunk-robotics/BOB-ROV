#!/bin/bash
# setup.sh

set -e # stop on any error

PI="bob-rov-rp5"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Copying setup script to Pi..."
scp "$SCRIPT_DIR/setup_py.sh" "$PI:/tmp/setup_py.sh"

echo "==> Running setup on Pi (this will take a while)..."
ssh -t "$PI" "bash /tmp/setup_py.sh"

echo "==> Waiting for Pi to reboot..."
sleep 30
ssh "$PI" "echo 'Pi is back online'"

echo "==> Setup complete, running deploy..."
"$SCRIPT_DIR/deploy.sh"

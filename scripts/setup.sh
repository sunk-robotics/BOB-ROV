#!/bin/bash
# setup.sh

set -e # stop on any error

PI="sunk@bob-rov-rp5"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Ubuntu Pro token is required for the realtime kernel.
# Prompts for it unless UBUNTU_PRO_TOKEN is already set in the environment.
if [ -z "${UBUNTU_PRO_TOKEN:-}" ]; then
  read -r -s -p "Enter Ubuntu Pro token: " UBUNTU_PRO_TOKEN
  echo # newline, since -s suppresses the one the user's Enter would print
fi

if [ -z "$UBUNTU_PRO_TOKEN" ]; then
  echo "Error: no Ubuntu Pro token provided" >&2
  exit 1
fi

echo "==> Copying setup script to Pi..."
scp "$SCRIPT_DIR/setup_pi.sh" "$PI:/tmp/setup_pi.sh"

echo "==> Running setup on Pi (this will take a while)..."
ssh -t "$PI" "UBUNTU_PRO_TOKEN='$UBUNTU_PRO_TOKEN' bash /tmp/setup_pi.sh"

echo "==> Waiting for Pi to reboot..."
sleep 30
ssh "$PI" "echo 'Pi is back online'"

echo "==> Setup complete, running deploy..."
"$SCRIPT_DIR/deploy.sh"

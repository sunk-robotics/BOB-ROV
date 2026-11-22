#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
rm compile_commands.json
jq -s 'add' build/*/compile_commands.json >compile_commands.json

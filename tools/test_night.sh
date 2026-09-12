#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p output/native
c++ -O2 -std=c++17 -Wall -Wextra -Inative/dashy native/tests/night_mode_test.cpp native/dashy/NightMode.cpp -o output/native/night-mode-test
python3 - <<'PY'
import subprocess,tempfile
with tempfile.TemporaryDirectory() as directory:
    subprocess.run(['output/native/night-mode-test',directory],check=True)
PY

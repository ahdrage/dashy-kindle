#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=.deps/kinduino/arduino
mkdir -p output/native
c++ -O2 -std=c++17 -Wall -Wextra -Inative/dashy -I"$sdk/libraries/ArduinoJson" \
    native/tests/netatmo_test.cpp native/dashy/Netatmo.cpp native/dashy/NetatmoStore.cpp \
    -o output/native/netatmo-test
c++ -O2 -std=c++17 -Wall -Wextra -pthread -Inative/dashy -I"$sdk/libraries/ArduinoJson" \
    -I"$sdk/cores/kindle" -I"$sdk/libraries/WiFi" \
    native/tests/netatmo_runtime_test.cpp native/dashy/Netatmo.cpp native/dashy/NetatmoStore.cpp \
    native/dashy/NetatmoRuntime.cpp "$sdk/cores/kindle/time.cpp" \
    -o output/native/netatmo-runtime-test
python3 - <<'PY'
from pathlib import Path
import json,shutil,subprocess,tempfile
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    subprocess.run(['output/native/netatmo-test',directory],check=True)
    (root/'assets').mkdir(); (root/'files').mkdir()
    shutil.copy2('assets/certs/DigiCertGlobalRootG2.crt',root/'assets/DigiCertGlobalRootG2.crt')
    (root/'files/netatmo.json').write_text(json.dumps({'client_id':'test','client_secret':'test','refresh_token':'initial-test'}))
    subprocess.run(['output/native/netatmo-runtime-test',directory],check=True,timeout=10)
PY

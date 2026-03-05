#!/bin/bash
## One-time setup for dependencies that can't be managed via idf_component.yml.
## Run this from the project root (platformio/) before first build.

set -e

echo "=== Cloning eez-framework ==="
if [ -d "components/eez-framework/.git" ]; then
    echo "Already cloned, skipping."
else
    git clone https://github.com/eez-open/eez-framework.git components/eez-framework
fi

echo "=== Installing ESP-IDF component CMakeLists.txt for eez-framework ==="
cp setup/eez_framework_CMakeLists.txt components/eez-framework/CMakeLists.txt

echo ""
echo "=== Setup complete ==="
echo "Next steps:"
echo "  1. source \$IDF_PATH/export.sh"
echo "  2. idf.py set-target esp32s3   (first time only)"
echo "  3. idf.py build"
echo "  4. idf.py -p /dev/cu.usbserial-* flash monitor"

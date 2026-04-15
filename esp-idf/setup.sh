#!/bin/bash
## One-time setup for dependencies that can't be managed via idf_component.yml.

set -e

echo "=== Cloning eez-framework ==="
if [ -d "components/eez-framework/.git" ]; then
    echo "Already cloned, skipping."
else
    git clone https://github.com/eez-open/eez-framework.git components/eez-framework
fi

echo "=== Installing ESP-IDF component CMakeLists.txt for eez-framework ==="
cp setup/eez_framework_CMakeLists.txt components/eez-framework/CMakeLists.txt

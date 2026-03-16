#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="build"
mkdir -p "$BUILD_DIR"

PASS=0
FAIL=0

run_test() {
    local src="$1"
    local name="${src%.cpp}"
    local bin="${BUILD_DIR}/${name}"

    printf "Building %s... " "$src"
    if c++ -std=c++17 "$src" -o "$bin" 2>/tmp/build_err; then
        printf "ok\n"
        if "$bin"; then
            PASS=$((PASS + 1))
        else
            FAIL=$((FAIL + 1))
        fi
    else
        printf "FAILED TO BUILD\n"
        cat /tmp/build_err
        FAIL=$((FAIL + 1))
    fi
    printf "\n"
}

for src in test_*.cpp; do
    run_test "$src"
done

echo "================================"
if [ "$FAIL" -eq 0 ]; then
    echo "All $PASS test suite(s) passed."
else
    echo "$PASS passed, $FAIL failed."
    exit 1
fi

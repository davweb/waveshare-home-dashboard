#!/opt/homebrew/bin/zsh

export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release;sdkconfig.local"
BUILD_DIR=build_release

idf.py -B ${BUILD_DIR} set-target esp32s3
idf.py -p /dev/cu.usbmodem* -B ${BUILD_DIR} flash

# Put things back after the release build
export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.local"
BUILD_DIR=build

idf.py -B ${BUILD_DIR} set-target esp32s3

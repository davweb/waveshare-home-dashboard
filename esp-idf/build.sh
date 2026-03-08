#!/opt/homebrew/bin/zsh

SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.local"
BUILD_DIR=build

idf.py -B ${BUILD_DIR} build

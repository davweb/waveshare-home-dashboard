#!/opt/homebrew/bin/zsh

# Set otherwise build time is wrong
export TZ=Europe/London

if ! command -v idf.py &>/dev/null;
then
    source ~/.espressif/v5.5.3/esp-idf/export.sh
fi

export "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.local"
export BUILD_DIR=build

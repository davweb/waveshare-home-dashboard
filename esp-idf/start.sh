#!/opt/homebrew/bin/zsh

# Set otherwise build time is wrong
export TZ=Europe/London

if ! command -v idf.py &>/dev/null;
then
    # Homebrew's python3 moves ahead of ESP-IDF; pin the env install.sh created
    export IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.5_py3.14_env
    source ~/.espressif/v5.5.3/esp-idf/export.sh
fi

export "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.local"
export BUILD_DIR=build

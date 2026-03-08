if [ -z "${IDF_PATH}" ]; then
    source /Users/dave/esp/esp-idf/export.sh
fi

export "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.local"

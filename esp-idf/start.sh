#!/opt/homebrew/bin/zsh

if (( ! $+aliases[idf.pf] ));
then
    source ~/.espressif/tools/activate_idf_v5.5.3.sh
fi

export "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.local"
export BUILD_DIR=build

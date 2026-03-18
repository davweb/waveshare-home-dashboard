#!/opt/homebrew/bin/zsh

source ./start.sh
idf.py -B ${BUILD_DIR} flash monitor

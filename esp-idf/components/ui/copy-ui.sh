
#!/opt/homebrew/bin/zsh

EEZ_STUDIO=/Applications/EEZ\ Studio.app/Contents/MacOS/EEZ\ Studio
GIT_DIR=$(git rev-parse --show-toplevel)
EEZ_PROJECT=${GIT_DIR}/eez-studio/waveshare-dashboard.eez-project
SRC_DIR=${GIT_DIR}/eez-studio/src/ui
DEST_DIR=${GIT_DIR}/esp-idf/components/ui

"${EEZ_STUDIO}" --build-project ${EEZ_PROJECT}

rm ${DEST_DIR}/*.c ${DEST_DIR}/*.h
cp ${SRC_DIR}/*.c ${SRC_DIR}/*.h ${DEST_DIR}


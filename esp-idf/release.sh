#!/opt/homebrew/bin/zsh

source ./start.sh

PROJECT_VER=$(date +"%Y-%m-%d-%H-%M")
FIRMWARE_DIR="../firmware"
SERVER="http://192.168.1.17"

export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.release;sdkconfig.local;sdkconfig.release_local"
BUILD_DIR=build_release

idf.py -DPROJECT_VER="${PROJECT_VER}" -B ${BUILD_DIR} set-target esp32s3
idf.py -DPROJECT_VER="${PROJECT_VER}" -p /dev/cu.usbmodem* -B ${BUILD_DIR} flash

# Copy firmware with versioned filename
mkdir -p ${FIRMWARE_DIR}
FIRMWARE_FILE="${FIRMWARE_DIR}/waveshare_dashboard-${PROJECT_VER}.bin"
cp ${BUILD_DIR}/waveshare_dashboard.bin ${FIRMWARE_FILE}
echo "Firmware saved to ${FIRMWARE_FILE}"

# Upload firmware to OTA server
echo "Uploading firmware version ${PROJECT_VER} to ${SERVER}..."
curl -f -X POST "${SERVER}/ota/firmware?version=${PROJECT_VER}" \
    --data-binary @${FIRMWARE_FILE} \
    -H "Content-Type: application/octet-stream"
echo "\nUpload complete."

# Put things back after the release build
export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.local"
BUILD_DIR=build_cli

idf.py -B ${BUILD_DIR} set-target esp32s3

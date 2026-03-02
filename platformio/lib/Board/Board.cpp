#include <DebugLog.h>
#include "Board.h"

using namespace esp_panel::drivers;

//This code is copied more or less verbatim from the main.cpp of the LVGL
//example code. I could tidy it up to match the board more specifically but it
//works so I'm leaving it alone.
//
//https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/examples/platformio/lvgl_v8_port/src/main.cpp

Board * initialiseBoard() {
    Board *board = new Board();
    board->init();

    #if LVGL_PORT_AVOID_TEAR
        auto lcd = board->getLCD();
        // When avoid tearing function is enabled, the frame buffer number
        // should be set in the board driver
        lcd->configFrameBufferNumber(LVGL_PORT_BUFFER_NUM);

        #if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
            auto lcd_bus = lcd->getBus();
            /**
             * As the anti-tearing feature typically consumes more PSRAM
             * bandwidth, for the ESP32-S3, we need to utilize the "bounce
             * buffer" functionality to enhance the RGB data bandwidth. This
             * feature will consume `bounce_buffer_size * bytes_per_pixel * 2`
             * of SRAM memory.
             */
            if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
                // Increase bounce buffer size to 20 lines to cope with Wifi load
                static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 20);
            }
        #endif
    #endif

    bool result = board->begin();

    if (result) {
        LOG_DEBUG("Board initialised successfully.");
    } else {
        LOG_ERROR("Board initialisation failed.");
    }

    return board;
}
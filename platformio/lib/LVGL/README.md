These are LVGL files taken from templates:

* `lvgl_v8_port.cpp`
  * Not modified at all
* `lvgl_v8_port.h`
  * `LVGL_PORT_BUFFER_MALLOC_CAPS` set to `MALLOC_CAP_SPIRAM` to enable PSRAM
  * `LVGL_PORT_BUFFER_SIZE_HEIGHT` set to 480 for a full size buffer

* `lv_conf.h`
  * `LV_COLOR_SCREEN_TRANSP` set to 0
  * `LV_MEMCPY_MEMSET_STD` set to 1
  * `LV_USE_PERF_MONITOR` set for debug builds
  * `LV_USE_LOG` only set for debug builds
  * Turning off fonts that aren't used
  * Turning off features that aren't used
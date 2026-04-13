#pragma once

#include "esp_http_server.h"

// Register the /system HTTP handler on an already-started server.
void register_data_handlers(httpd_handle_t server);

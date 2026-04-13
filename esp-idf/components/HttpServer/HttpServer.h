#pragma once

#include "HtmlUtils.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// Start the HTTP server and register built-in handlers (/, /log, /screenshot).
// Returns the server handle, or NULL on failure.
httpd_handle_t startHttpServer(void);

#ifdef __cplusplus
}
#endif

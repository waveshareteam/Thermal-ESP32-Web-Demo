/*****************************************************************************
 * @file     thermal_web.h
 * @version  1.0
 * @brief    Header file for thermal web streaming server
 * @date     16 Jul 2026
 ******************************************************************************/
#ifndef COMPONENTS_NET_INCLUDE_THERMAL_WEB_H_
#define COMPONENTS_NET_INCLUDE_THERMAL_WEB_H_

#include <esp_http_server.h>

/**
 * @brief Initialize thermal web server handlers on an existing HTTP server
 * @param server  Handle to the already-running HTTP server
 */
void thermal_web_init(httpd_handle_t server);

#endif /* COMPONENTS_NET_INCLUDE_THERMAL_WEB_H_ */

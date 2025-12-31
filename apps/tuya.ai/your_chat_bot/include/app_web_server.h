/**
 * @file app_web_server.h
 * @brief Simple HTTP web server for chat history display
 * @version 0.1
 * @date 2025-12-05
 */

#ifndef APP_WEB_SERVER_H
#define APP_WEB_SERVER_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define WEB_SERVER_PORT 8080
#define WEB_SERVER_MAX_CONNECTIONS 5

/***********************************************************
***********************function define***********************
***********************************************************/

/**
 * @brief Initialize and start web server
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_web_server_init(void);

/**
 * @brief Stop web server
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_web_server_deinit(void);

/**
 * @brief Notify all connected clients about new message
 * This should be called when a new message is added to chat history
 */
void app_web_server_notify_new_message(void);

#ifdef __cplusplus
}
#endif

#endif // APP_WEB_SERVER_H



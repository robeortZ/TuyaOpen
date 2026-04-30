/**
 * @file buddy_http.h
 * @brief HTTP polling client for buddy-bridge server.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUDDY_HTTP_H__
#define __BUDDY_HTTP_H__

#include "tuya_cloud_types.h"
#include "buddy_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the HTTP client with bridge server coordinates.
 * @param host  Bridge server hostname or IP string.
 * @param port  Bridge server TCP port.
 * @return OPRT_OK on success.
 */
OPERATE_RET buddy_http_init(const char *host, uint16_t port);

/**
 * @brief Poll GET /api/state and fill @p info.
 * @return OPRT_OK if the request succeeded and JSON parsed correctly.
 *         OPRT_COM_ERROR on network or parse failure.
 */
OPERATE_RET buddy_http_poll_state(buddy_state_info_t *info);

/**
 * @brief Post an event (approve/deny/navigate/shake) to POST /api/event.
 * @param event_type  One of: "approve", "deny", "navigate", "shake".
 * @param action_id   Pending action ID string (may be empty "").
 * @return OPRT_OK on success.
 */
OPERATE_RET buddy_http_post_event(const char *event_type, const char *action_id);

#ifdef __cplusplus
}
#endif

#endif /* __BUDDY_HTTP_H__ */

/**
 * @file buddy_ws.h
 * @brief WebSocket client for bridge connection.
 *
 * Replaces buddy_http: instead of HTTPS polling, the device opens a plain-TCP
 * WebSocket connection to ws://BRIDGE_HOST:BRIDGE_PORT/device.  This avoids
 * the mbedTLS handshake issue entirely — no TLS is involved.
 *
 * Protocol:
 *   Bridge → Device : {"type":"state_update","state":"BUSY","sessions":3,
 *                       "tokens":1234,"message":"...","action":{...}}
 *   Device → Bridge : {"type":"event","event":"approve","action_id":"abc123"}
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUDDY_WS_H__
#define __BUDDY_WS_H__

#include "tuya_cloud_types.h"
#include "buddy_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback invoked (from the WS background thread) whenever the bridge
 *        pushes a new state_update.  The caller MUST NOT call buddy_fsm_*
 *        directly from this callback — copy the info and handle it in the
 *        main task to avoid cross-thread issues.
 */
typedef void (*buddy_ws_state_cb_t)(const buddy_state_info_t *info);

/**
 * @brief Initialise and start the WebSocket client background task.
 * @param[in] host  Bridge server IP / hostname (e.g. "192.168.1.100")
 * @param[in] port  Bridge server port (e.g. 8765)
 * @param[in] cb    State-update callback; called on every push from the bridge
 * @return OPRT_OK on success.
 */
OPERATE_RET buddy_ws_init(const char *host, uint16_t port, buddy_ws_state_cb_t cb);

/**
 * @brief Send a device event to the bridge over the WebSocket connection.
 *        Thread-safe; may be called from any task.
 * @param[in] event_type  "approve" | "deny" | "navigate" | "shake"
 * @param[in] action_id   Action UUID; NULL or "" when not applicable
 * @return OPRT_OK on success, OPRT_RESOURCE_NOT_READY if not connected.
 */
OPERATE_RET buddy_ws_send_event(const char *event_type, const char *action_id);

/**
 * @brief Query whether the WebSocket connection is currently established.
 */
bool buddy_ws_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUDDY_WS_H__ */

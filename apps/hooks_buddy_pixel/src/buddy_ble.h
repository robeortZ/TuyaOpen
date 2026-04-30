/**
 * @file buddy_ble.h
 * @brief BLE peripheral channel for buddy_pixel.
 *
 * The device advertises as "BuddyPixel" with the Tuya BLE service (0xFDFD/
 * 0xFD50). buddy_bridge connects, writes JSON state-update lines to the write
 * characteristic, and subscribes to the notify characteristic for events
 * (approve / deny / navigate / shake) sent back by the device.
 *
 * JSON protocol — identical to the WebSocket channel:
 *   Bridge → Device (write char): {"type":"state_update","state":"BUSY",...}\n
 *   Device → Bridge (notify char): {"type":"event","event":"approve","action_id":"..."}\n
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUDDY_BLE_H__
#define __BUDDY_BLE_H__

#include "tuya_cloud_types.h"
#include "buddy_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Callback type — same signature as buddy_ws_state_cb_t */
typedef void (*buddy_ble_state_cb_t)(const buddy_state_info_t *info);

/**
 * @brief Initialise the BLE peripheral stack and begin advertising.
 * @param cb  Called every time a state_update message is received.
 */
OPERATE_RET buddy_ble_init(buddy_ble_state_cb_t cb);

/**
 * @brief Send an event notification to the connected central (buddy_bridge).
 *        Transmits {"type":"event","event":"<event_type>","action_id":"<id>"}\n
 * @return OPRT_OK on success, OPRT_RESOURCE_NOT_READY if not connected / no
 *         subscriber.
 */
OPERATE_RET buddy_ble_send_event(const char *event_type, const char *action_id);

/** @return true when a BLE central is connected and subscribed. */
bool buddy_ble_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUDDY_BLE_H__ */

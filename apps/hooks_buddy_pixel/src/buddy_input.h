/**
 * @file buddy_input.h
 * @brief Button input handler for buddy_pixel.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUDDY_INPUT_H__
#define __BUDDY_INPUT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Event types emitted by the input layer */
typedef enum {
    BUDDY_INPUT_NONE = 0,
    BUDDY_INPUT_APPROVE,        /* OK  single click  — confirm permission request */
    BUDDY_INPUT_DENY,           /* B   single click  — reject permission request  */
    BUDDY_INPUT_NAVIGATE,       /* A   single click  — navigate                   */
    BUDDY_INPUT_SHAKE,          /* B   long press    — dizzy animation            */
    BUDDY_INPUT_CLEAR_CONFIG,   /* OK  long press    — erase NVS config + reboot  */
} buddy_input_event_e;

/** Callback invoked from button ISR context (keep it short) */
typedef void (*buddy_input_cb_t)(buddy_input_event_e event);

/**
 * @brief Register buttons and install callback.
 * @param cb  Function called on each input event.
 * @return OPRT_OK on success.
 */
OPERATE_RET buddy_input_init(buddy_input_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __BUDDY_INPUT_H__ */

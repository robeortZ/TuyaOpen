/**
 * @file buddy_input.c
 * @brief Button registration for OK / A / B keys.
 *   OK  → approve      (single click)   ← confirm Claude permission request
 *   OK  → clear config (long press)     ← erase NVS WiFi config + reboot
 *   A   → navigate     (single click)
 *   B   → deny         (single click)   ← reject Claude permission request
 *   B   → shake        (long press)     ← dizzy animation
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "buddy_input.h"
#include "tdl_button_manage.h"
#include "tal_log.h"
#include <string.h>

/***********************************************************
 * Constants (board-specific button names from tuya_kconfig.h)
 *   BUTTON_NAME   = "button1"  → OK
 *   BUTTON_NAME_2 = "button2"  → A
 *   BUTTON_NAME_3 = "button3"  → B
 ***********************************************************/
#ifndef BUTTON_NAME
#define BUTTON_NAME   "button1"
#endif
#ifndef BUTTON_NAME_2
#define BUTTON_NAME_2 "button2"
#endif
#ifndef BUTTON_NAME_3
#define BUTTON_NAME_3 "button3"
#endif

#define BTN_OK  BUTTON_NAME
#define BTN_A   BUTTON_NAME_2
#define BTN_B   BUTTON_NAME_3

/***********************************************************
 * Internal state
 ***********************************************************/
static buddy_input_cb_t  s_cb = NULL;

static TDL_BUTTON_HANDLE s_ok = NULL;
static TDL_BUTTON_HANDLE s_a  = NULL;
static TDL_BUTTON_HANDLE s_b  = NULL;

/***********************************************************
 * Button callbacks
 ***********************************************************/
static void cb_ok(char *name, TDL_BUTTON_TOUCH_EVENT_E ev, void *arg)
{
    (void)name; (void)arg;
    if (ev == TDL_BUTTON_PRESS_SINGLE_CLICK && s_cb) {
        s_cb(BUDDY_INPUT_APPROVE);
    } else if (ev == TDL_BUTTON_LONG_PRESS_START && s_cb) {
        /* Long-press OK clears WiFi config and reboots into provisioning mode */
        s_cb(BUDDY_INPUT_CLEAR_CONFIG);
    }
}

static void cb_a(char *name, TDL_BUTTON_TOUCH_EVENT_E ev, void *arg)
{
    (void)name; (void)arg;
    if (ev == TDL_BUTTON_PRESS_SINGLE_CLICK && s_cb) {
        s_cb(BUDDY_INPUT_NAVIGATE);
    }
}

static void cb_b(char *name, TDL_BUTTON_TOUCH_EVENT_E ev, void *arg)
{
    (void)name; (void)arg;
    if (ev == TDL_BUTTON_PRESS_SINGLE_CLICK && s_cb) {
        s_cb(BUDDY_INPUT_DENY);
    } else if (ev == TDL_BUTTON_LONG_PRESS_START && s_cb) {
        /* Long-press B triggers shake/dizzy */
        s_cb(BUDDY_INPUT_SHAKE);
    }
}

/***********************************************************
 * Public API
 ***********************************************************/
OPERATE_RET buddy_input_init(buddy_input_cb_t cb)
{
    s_cb = cb;

    TDL_BUTTON_CFG_T cfg = {
        .long_start_valid_time    = 1500,
        .long_keep_timer          = 500,
        .button_debounce_time     = 50,
        .button_repeat_valid_count = 2,
        .button_repeat_valid_time  = 400,
    };

    OPERATE_RET rt;

    rt = tdl_button_create(BTN_OK, &cfg, &s_ok);
    if (rt == OPRT_OK) {
        tdl_button_event_register(s_ok, TDL_BUTTON_PRESS_SINGLE_CLICK, cb_ok);
        tdl_button_event_register(s_ok, TDL_BUTTON_LONG_PRESS_START,   cb_ok);
    } else {
        PR_WARN("buddy_input: OK button create failed: %d", rt);
    }

    rt = tdl_button_create(BTN_A, &cfg, &s_a);
    if (rt == OPRT_OK) {
        tdl_button_event_register(s_a, TDL_BUTTON_PRESS_SINGLE_CLICK, cb_a);
    } else {
        PR_WARN("buddy_input: A button create failed: %d", rt);
    }

    rt = tdl_button_create(BTN_B, &cfg, &s_b);
    if (rt == OPRT_OK) {
        tdl_button_event_register(s_b, TDL_BUTTON_PRESS_SINGLE_CLICK, cb_b);
        tdl_button_event_register(s_b, TDL_BUTTON_LONG_PRESS_START,   cb_b);
    } else {
        PR_WARN("buddy_input: B button create failed: %d", rt);
    }

    PR_NOTICE("buddy_input: initialized (OK/A/B)");
    return OPRT_OK;
}

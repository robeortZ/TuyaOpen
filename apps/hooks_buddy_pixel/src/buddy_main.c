/**
 * @file buddy_main.c
 * @brief Claude Desktop Buddy - main entry point.
 *
 * Flow:
 *  1. TuyaOS runtime init (log / KV / timers / workqueue)
 *  2. [if ENABLE_WIFI_WS] Load config from NVS; SoftAP provisioning; WiFi
 *  3. Board hardware register (pixel LED, buttons, buzzer)
 *  4. buddy_fsm / buddy_ble / buddy_input init
 *  5. Main loop: render animations, handle BLE state updates and button events
 *
 * To re-enable WebSocket bridge mode, set ENABLE_WIFI_WS to 1.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

/* ---- Feature switch ---- */
#define ENABLE_WIFI_WS  0   /* 0 = BLE only;  1 = BLE + WiFi/WS bridge */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tal_kv.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tkl_output.h"
#include "board_com_api.h"
#include "board_buzzer_api.h"

#include "buddy_fsm.h"
#include "buddy_ble.h"
#include "buddy_input.h"

#if ENABLE_WIFI_WS
#include "tal_wifi.h"
#include "buddy_ws.h"
#include "buddy_config.h"
#endif

#include <string.h>
#include <stdio.h>

/***********************************************************
 * Kconfig defaults (used only in WiFi/WS mode)
 ***********************************************************/
#if ENABLE_WIFI_WS
#ifndef CONFIG_BUDDY_WIFI_SSID
#define CONFIG_BUDDY_WIFI_SSID     "your_wifi_ssid"
#endif
#ifndef CONFIG_BUDDY_WIFI_PASSWORD
#define CONFIG_BUDDY_WIFI_PASSWORD ""
#endif
#ifndef CONFIG_BUDDY_BRIDGE_HOST
#define CONFIG_BUDDY_BRIDGE_HOST   "192.168.1.100"
#endif
#ifndef CONFIG_BUDDY_BRIDGE_PORT
#define CONFIG_BUDDY_BRIDGE_PORT   8765
#endif
#endif /* ENABLE_WIFI_WS */

/***********************************************************
 * Internal state
 ***********************************************************/
#if ENABLE_WIFI_WS
static volatile bool s_wifi_ready  = false;
static volatile bool s_wifi_failed = false;
#endif

static THREAD_HANDLE s_buddy_thrd = NULL;

/* Pending event from buttons (set in callback, consumed in task) */
static volatile buddy_input_event_e s_pending_event = BUDDY_INPUT_NONE;
/* Action ID of the current pending permission request */
static char s_pending_action_id[BUDDY_ACTION_ID_LEN] = {0};

/* State update pushed by a background thread; consumed in buddy_task */
static volatile bool      s_state_updated = false;
static buddy_state_info_t s_state_buf     = {0};

/***********************************************************
 * WiFi event callback (WiFi/WS mode only)
 ***********************************************************/
#if ENABLE_WIFI_WS
static void wifi_event_cb(WF_EVENT_E event, void *arg)
{
    switch (event) {
    case WFE_CONNECTED:
        PR_NOTICE("buddy: WiFi connected");
        s_wifi_ready  = true;
        s_wifi_failed = false;
        break;
    case WFE_CONNECT_FAILED:
        PR_ERR("buddy: WiFi connection failed");
        s_wifi_failed = true;
        break;
    case WFE_DISCONNECTED:
        PR_WARN("buddy: WiFi disconnected");
        s_wifi_ready = false;
        break;
    default:
        break;
    }
}
#endif /* ENABLE_WIFI_WS */

/***********************************************************
 * Input event callback (called from button driver context)
 ***********************************************************/
static void input_event_cb(buddy_input_event_e ev)
{
    s_pending_event = ev;
}

/***********************************************************
 * Buzzer helpers
 ***********************************************************/
static void buzz_attention(void)
{
    board_buzzer_play_note_duration(NOTE_A5, 80);
    tal_system_sleep(40);
    board_buzzer_play_note_duration(NOTE_A5, 80);
}

static void buzz_approve(void)
{
    board_buzzer_play_note_duration(NOTE_C5, 60);
    tal_system_sleep(30);
    board_buzzer_play_note_duration(NOTE_E5, 120);
}

static void buzz_deny(void)
{
    board_buzzer_play_note_duration(NOTE_E4, 120);
}

static const char *s_state_names[] = {
    "DISCONNECTED", "IDLE", "BUSY", "ATTENTION", "CELEBRATE", "DIZZY"
};
#define STATE_NAME(s) \
    ((s) < BUDDY_STATE_MAX ? s_state_names[(s)] : "UNKNOWN")

/***********************************************************
 * State callback (shared by BLE and WS)
 ***********************************************************/
static void state_cb(const buddy_state_info_t *info)
{
    PR_NOTICE("buddy: state update → %s", STATE_NAME(info->state));
    s_state_buf     = *info;
    s_state_updated = true;
}

/***********************************************************
 * Main buddy task
 ***********************************************************/
static void buddy_task(void *arg)
{
    PR_NOTICE("buddy: task started");

    buddy_state_e last_state     = BUDDY_STATE_MAX; /* force first render */
    uint32_t      last_render_ms = 0;

    buddy_state_info_t cur_info = {.state = BUDDY_STATE_DISCONNECTED};
    buddy_fsm_set_state(&cur_info);

    for (;;) {
        uint32_t now_ms = tal_system_get_millisecond();

        /* ---- Render tick ---- */
        if ((now_ms - last_render_ms) >= BUDDY_RENDER_INTERVAL_MS) {
            last_render_ms = now_ms;
            buddy_fsm_render_tick();

            if (buddy_fsm_get_state() == BUDDY_STATE_ATTENTION) {
                static uint32_t last_beep = 0;
                if (now_ms - last_beep >= 3000) {
                    last_beep = now_ms;
                    buzz_attention();
                }
            }
        }

        /* ---- Apply state update (pushed by BLE/WS background thread) ---- */
        if (s_state_updated) {
            s_state_updated = false;
            buddy_state_info_t info = s_state_buf;

            if (info.action.valid) {
                strncpy(s_pending_action_id, info.action.id,
                        sizeof(s_pending_action_id) - 1);
            } else {
                s_pending_action_id[0] = '\0';
            }

            if (info.state == BUDDY_STATE_ATTENTION &&
                last_state != BUDDY_STATE_ATTENTION) {
                buzz_attention();
            }
            last_state = info.state;

            buddy_fsm_set_state(&info);
        }

        /* ---- Handle input events ---- */
        buddy_input_event_e ev = s_pending_event;
        if (ev != BUDDY_INPUT_NONE) {
            s_pending_event = BUDDY_INPUT_NONE;
            switch (ev) {
            case BUDDY_INPUT_APPROVE: {
                buzz_approve();
#if ENABLE_WIFI_WS
                OPERATE_RET ws_rt  = buddy_ws_send_event("approve", s_pending_action_id);
#endif
                OPERATE_RET ble_rt = buddy_ble_send_event("approve", s_pending_action_id);
#if ENABLE_WIFI_WS
                PR_NOTICE("buddy: APPROVE sent (ws=%d ble=%d)", ws_rt, ble_rt);
                if (ws_rt == OPRT_OK || ble_rt == OPRT_OK)
#else
                PR_NOTICE("buddy: APPROVE sent (ble=%d)", ble_rt);
                if (ble_rt == OPRT_OK)
#endif
                {
                    s_pending_action_id[0] = '\0';
                    if (buddy_fsm_get_state() == BUDDY_STATE_ATTENTION) {
                        buddy_state_info_t idle = {.state = BUDDY_STATE_IDLE};
                        buddy_fsm_set_state(&idle);
                    }
                }
                break;
            }
            case BUDDY_INPUT_DENY: {
                buzz_deny();
#if ENABLE_WIFI_WS
                OPERATE_RET ws_rt  = buddy_ws_send_event("deny", s_pending_action_id);
#endif
                OPERATE_RET ble_rt = buddy_ble_send_event("deny", s_pending_action_id);
#if ENABLE_WIFI_WS
                PR_NOTICE("buddy: DENY sent (ws=%d ble=%d)", ws_rt, ble_rt);
                if (ws_rt == OPRT_OK || ble_rt == OPRT_OK)
#else
                PR_NOTICE("buddy: DENY sent (ble=%d)", ble_rt);
                if (ble_rt == OPRT_OK)
#endif
                {
                    s_pending_action_id[0] = '\0';
                    if (buddy_fsm_get_state() == BUDDY_STATE_ATTENTION) {
                        buddy_state_info_t idle = {.state = BUDDY_STATE_IDLE};
                        buddy_fsm_set_state(&idle);
                    }
                }
                break;
            }
            case BUDDY_INPUT_NAVIGATE:
                if (s_pending_action_id[0] != '\0' &&
                    buddy_fsm_get_state() == BUDDY_STATE_ATTENTION) {
                    /* A button in ATTENTION = "approve always" */
                    buzz_approve();
#if ENABLE_WIFI_WS
                    OPERATE_RET ws_rt  = buddy_ws_send_event("navigate", s_pending_action_id);
#endif
                    OPERATE_RET ble_rt = buddy_ble_send_event("navigate", s_pending_action_id);
#if ENABLE_WIFI_WS
                    PR_NOTICE("buddy: APPROVE_ALWAYS sent (ws=%d ble=%d)", ws_rt, ble_rt);
                    if (ws_rt == OPRT_OK || ble_rt == OPRT_OK)
#else
                    PR_NOTICE("buddy: APPROVE_ALWAYS sent (ble=%d)", ble_rt);
                    if (ble_rt == OPRT_OK)
#endif
                    {
                        s_pending_action_id[0] = '\0';
                        if (buddy_fsm_get_state() == BUDDY_STATE_ATTENTION) {
                            buddy_state_info_t idle = {.state = BUDDY_STATE_IDLE};
                            buddy_fsm_set_state(&idle);
                        }
                    }
                } else {
#if ENABLE_WIFI_WS
                    buddy_ws_send_event("navigate", "");
#endif
                    buddy_ble_send_event("navigate", "");
                    PR_NOTICE("buddy: NAVIGATE sent");
                }
                break;
            case BUDDY_INPUT_SHAKE:
                buddy_fsm_trigger_dizzy();
#if ENABLE_WIFI_WS
                buddy_ws_send_event("shake", "");
#endif
                buddy_ble_send_event("shake", "");
                PR_NOTICE("buddy: SHAKE sent");
                break;
            case BUDDY_INPUT_CLEAR_CONFIG:
                PR_NOTICE("buddy: OK long-press — clearing config and rebooting");
                board_buzzer_play_note_duration(NOTE_G5, 80);
                tal_system_sleep(60);
                board_buzzer_play_note_duration(NOTE_E5, 80);
                tal_system_sleep(60);
                board_buzzer_play_note_duration(NOTE_C5, 160);
                tal_system_sleep(300);
#if ENABLE_WIFI_WS
                buddy_config_clear();
#endif
                tal_system_reset();
                break;
            default:
                break;
            }
        }

        tal_system_sleep(10);
    }
}

/***********************************************************
 * Application entry point
 ***********************************************************/
void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 2 * 1024,
                 (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("=== Buddy Pixel v1.0.0 (BLE%s) ===",
              ENABLE_WIFI_WS ? "+WiFi/WS" : " only");

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "buddypixel1234xx",
        .key  = "bpkeystore567890",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tal_time_service_init();

#if ENABLE_WIFI_WS
    /* --- Load config from NVS (or enter provisioning mode) --- */
    buddy_config_t cfg = {0};
    bool have_cfg = buddy_config_load(&cfg);
    if (!have_cfg) {
        strncpy(cfg.ssid, CONFIG_BUDDY_WIFI_SSID,    BUDDY_CFG_SSID_LEN);
        strncpy(cfg.pass, CONFIG_BUDDY_WIFI_PASSWORD, BUDDY_CFG_PASS_LEN);
        strncpy(cfg.host, CONFIG_BUDDY_BRIDGE_HOST,   BUDDY_CFG_HOST_LEN);
        cfg.port = CONFIG_BUDDY_BRIDGE_PORT;
    }

    if (!have_cfg && (cfg.ssid[0] == '\0' ||
                      strcmp(cfg.ssid, "your_wifi_ssid") == 0)) {
        PR_NOTICE("buddy: no NVS config — starting SoftAP provisioning");
        tal_wifi_init(wifi_event_cb);
        buddy_config_run_server();
        PR_WARN("buddy: provisioning timeout — continuing with Kconfig defaults");
        buddy_config_load(&cfg);
    }

    PR_NOTICE("buddy: WiFi SSID='%s'  Bridge=%s:%u",
              cfg.ssid, cfg.host, (unsigned)cfg.port);

    tal_wifi_init(wifi_event_cb);
    tal_wifi_set_work_mode(WWM_STATION);
    tal_wifi_station_connect((int8_t *)cfg.ssid, (int8_t *)cfg.pass);

    PR_NOTICE("buddy: connecting WiFi...");
    uint32_t wifi_timeout = 0;
    while (!s_wifi_ready && !s_wifi_failed && wifi_timeout < 30000) {
        tal_system_sleep(200);
        wifi_timeout += 200;
    }
    if (!s_wifi_ready) {
        PR_ERR("buddy: WiFi failed/timed-out — continuing in offline mode");
    }
#endif /* ENABLE_WIFI_WS */

    /* --- Board hardware (pixels, buttons, buzzer) --- */
    OPERATE_RET rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("buddy: board_register_hardware failed: %d", rt);
    }

    /* --- Sub-modules --- */
    buddy_fsm_init();
    buddy_input_init(input_event_cb);
#if ENABLE_WIFI_WS
    buddy_ws_init(cfg.host, cfg.port, state_cb);
#endif
    buddy_ble_init(state_cb);

    /* --- Startup chime --- */
    board_buzzer_play_note_duration(NOTE_C5, 80);
    tal_system_sleep(40);
    board_buzzer_play_note_duration(NOTE_G5, 160);

    /* --- Launch buddy task --- */
    THREAD_CFG_T tcfg = {
        .stackDepth = 1024 * 8,
        .priority   = THREAD_PRIO_2,
        .thrdname   = "buddy",
    };
    tal_thread_create_and_start(&s_buddy_thrd, NULL, NULL, buddy_task, NULL, &tcfg);

    for (;;) {
        tal_system_sleep(1000);
    }
}

/* ---------- Platform entry points ---------- */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[]) { user_main(); }
#else
static THREAD_HANDLE s_app_thrd = NULL;
static void app_thread(void *arg)
{
    user_main();
    tal_thread_delete(s_app_thrd);
    s_app_thrd = NULL;
}
void tuya_app_main(void)
{
    THREAD_CFG_T p = {1024*8, THREAD_PRIO_1, "tuya_app"};
    tal_thread_create_and_start(&s_app_thrd, NULL, NULL, app_thread, NULL, &p);
}
#endif

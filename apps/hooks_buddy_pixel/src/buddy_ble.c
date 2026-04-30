/**
 * @file buddy_ble.c
 * @brief BLE peripheral implementation for buddy_pixel.
 *
 * Uses the raw TAL BLE API (tal_ble_*) — ENABLE_BT_SERVICE must be disabled
 * so the higher-level Tuya BLE service does not also initialise the stack.
 *
 * Advertising:
 *   - AD Flags: LE General Discoverable, no BR/EDR
 *   - Incomplete 16-bit UUID list: 0xFDFD  (Tuya scan UUID)
 *   - Service Data: UUID 0xFD50
 *   Scan response:
 *   - Manufacturer Specific: company 0x07D0 (Tuya)
 *   - Complete Local Name: "BuddyPixel"
 *
 * GATT service (created by tal_ble_bt_init):
 *   Service    0x0000FD50-0000-1000-8000-00805f9b34fb  (Tuya CMD V2)
 *   Write  [0] 0x00000001-0000-1001-8001-00805f9b07d0
 *   Notify [1] 0x00000002-0000-1001-8001-00805f9b07d0
 *   Read   [2] 0x00000003-0000-1001-8001-00805f9b07d0
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "buddy_ble.h"
#include "tal_bluetooth.h"
#include "tal_log.h"
#include "tal_system.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── Receive line buffer ───────────────────────────────────────────────────*/
#define BLE_LINE_BUF_SIZE 512

static char   s_rx_buf[BLE_LINE_BUF_SIZE];
static size_t s_rx_len = 0;

/* ─── Connection state ──────────────────────────────────────────────────────*/
static buddy_ble_state_cb_t s_state_cb       = NULL;
static volatile bool        s_connected      = false;
static volatile bool        s_notify_enabled = false;
static TAL_BLE_PEER_INFO_T  s_peer;

/* ─── Advertising payloads ──────────────────────────────────────────────────*/

/* ADV_IND: Flags + Incomplete 16-bit UUID (0xFDFD) + Service Data (0xFD50) */
static const uint8_t s_adv_data[] = {
    0x02, 0x01, 0x06,           /* Flags: LE General Discoverable, no BR/EDR */
    0x03, 0x02, 0xFD, 0xFD,     /* Incomplete list of 16-bit UUIDs: 0xFDFD   */
    0x04, 0x16, 0x50, 0xFD,     /* Service Data: UUID 0xFD50                 */
    0x00,                       /* (minimal service data payload)             */
};

/* SCAN_RSP: Manufacturer Data (Tuya) + Complete Local Name "BuddyPixel" */
static const uint8_t s_rsp_data[] = {
    0x03, 0xFF, 0xD0, 0x07,     /* Manufacturer Specific: company 0x07D0     */
    0x0B, 0x09,                 /* Complete Local Name, length 11             */
    'B','u','d','d','y','P','i','x','e','l',
};

/* ─── JSON dispatch (same logic as buddy_ws.c __dispatch_msg) ───────────────*/

static buddy_state_e __parse_state(const char *s)
{
    if (!s)                              return BUDDY_STATE_IDLE;
    if (strcmp(s, "IDLE")         == 0) return BUDDY_STATE_IDLE;
    if (strcmp(s, "BUSY")         == 0) return BUDDY_STATE_BUSY;
    if (strcmp(s, "ATTENTION")    == 0) return BUDDY_STATE_ATTENTION;
    if (strcmp(s, "CELEBRATE")    == 0) return BUDDY_STATE_CELEBRATE;
    if (strcmp(s, "DIZZY")        == 0) return BUDDY_STATE_DIZZY;
    if (strcmp(s, "DISCONNECTED") == 0) return BUDDY_STATE_DISCONNECTED;
    return BUDDY_STATE_IDLE;
}

static void __dispatch_line(const char *line, size_t len)
{
    if (!line || len == 0 || !s_state_cb) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength(line, len);
    if (!root) {
        PR_WARN("buddy_ble: JSON parse error: %.40s", line);
        return;
    }

    cJSON *type_j = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type_j)) {
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type_j->valuestring, "state_update") == 0) {
        buddy_state_info_t info = {0};
        cJSON *j;

        j = cJSON_GetObjectItem(root, "state");
        info.state = __parse_state(cJSON_IsString(j) ? j->valuestring : NULL);

        j = cJSON_GetObjectItem(root, "sessions");
        info.session_count = cJSON_IsNumber(j) ? (uint32_t)j->valuedouble : 0;

        j = cJSON_GetObjectItem(root, "tokens");
        info.token_count = cJSON_IsNumber(j) ? (uint32_t)j->valuedouble : 0;

        j = cJSON_GetObjectItem(root, "message");
        if (cJSON_IsString(j)) {
            strncpy(info.message, j->valuestring, sizeof(info.message) - 1);
        }

        cJSON *action = cJSON_GetObjectItem(root, "action");
        if (cJSON_IsObject(action)) {
            j = cJSON_GetObjectItem(action, "id");
            if (cJSON_IsString(j) && j->valuestring[0] != '\0') {
                strncpy(info.action.id, j->valuestring,
                        sizeof(info.action.id) - 1);
                cJSON *p = cJSON_GetObjectItem(action, "prompt");
                if (cJSON_IsString(p)) {
                    strncpy(info.action.prompt, p->valuestring,
                            sizeof(info.action.prompt) - 1);
                }
                info.action.valid = true;
            }
        }

        s_state_cb(&info);
    }

    cJSON_Delete(root);
}

/* ─── BLE write — accumulate bytes, dispatch on newline ────────────────────*/

static void __rx_feed(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (s_rx_len < BLE_LINE_BUF_SIZE - 1) {
            s_rx_buf[s_rx_len++] = c;
        }
        if (c == '\n') {
            s_rx_buf[s_rx_len] = '\0';
            __dispatch_line(s_rx_buf, s_rx_len);
            s_rx_len = 0;
        }
    }
}

/* ─── BLE event callback ────────────────────────────────────────────────────*/

static void __ble_event_cb(TAL_BLE_EVT_PARAMS_T *p_event)
{
    switch (p_event->type) {

    case TAL_BLE_STACK_INIT:
        if (p_event->ble_event.init == 0) {
            PR_NOTICE("buddy_ble: stack ready — starting advertising");
            TAL_BLE_DATA_T adv = {
                .p_data = (uint8_t *)s_adv_data,
                .len    = (uint16_t)sizeof(s_adv_data),
            };
            TAL_BLE_DATA_T rsp = {
                .p_data = (uint8_t *)s_rsp_data,
                .len    = (uint16_t)sizeof(s_rsp_data),
            };
            tal_ble_advertising_data_set(&adv, &rsp);
            tal_ble_advertising_start(TUYAOS_BLE_DEFAULT_ADV_PARAM);
        }
        break;

    case TAL_BLE_EVT_PERIPHERAL_CONNECT:
        if (p_event->ble_event.connect.result == 0) {
            s_peer      = p_event->ble_event.connect.peer;
            s_connected = true;
            PR_NOTICE("buddy_ble: central connected");

            /* Request a longer supervision timeout (32 s, the BLE spec maximum).
             * Default (0x100 = 2.56 s) is too short to survive WiFi/BLE coexistence
             * radio-sharing gaps on BK7258. */
            TAL_BLE_CONN_PARAMS_T params = {
                .min_conn_interval = 24,      /* 30 ms  */
                .max_conn_interval = 48,      /* 60 ms  */
                .latency           = 0,
                .conn_sup_timeout  = 0x0C80,  /* 32 s (maximum allowed by spec) */
            };
            tal_ble_conn_param_update(s_peer, &params);
        }
        break;

    case TAL_BLE_EVT_DISCONNECT:
        s_connected      = false;
        s_notify_enabled = false;
        s_rx_len         = 0;   /* flush partial line */
        PR_NOTICE("buddy_ble: central disconnected — restarting advertising");
        /* Do NOT push DISCONNECTED here: hooks intentionally connect, write,
         * then disconnect.  The last received state should persist on screen. */
        tal_ble_advertising_start(TUYAOS_BLE_DEFAULT_ADV_PARAM);
        break;

    case TAL_BLE_EVT_SUBSCRIBE:
        s_notify_enabled = (p_event->ble_event.subscribe.cur_notify != 0);
        PR_NOTICE("buddy_ble: notify %s",
                  s_notify_enabled ? "enabled" : "disabled");
        break;

    case TAL_BLE_EVT_MTU_REQUEST:
        /* Accept the MTU offered by the central */
        tal_ble_server_exchange_mtu_reply(s_peer,
                                          p_event->ble_event.exchange_mtu.mtu);
        PR_DEBUG("buddy_ble: MTU = %u", p_event->ble_event.exchange_mtu.mtu);
        break;

    case TAL_BLE_EVT_WRITE_REQ: {
        const TAL_BLE_DATA_T *rep = &p_event->ble_event.write_report.report;
        __rx_feed(rep->p_data, rep->len);
        break;
    }

    default:
        break;
    }
}

/* ─── Public API ────────────────────────────────────────────────────────────*/

OPERATE_RET buddy_ble_init(buddy_ble_state_cb_t cb)
{
    if (!cb) {
        return OPRT_INVALID_PARM;
    }
    s_state_cb       = cb;
    s_connected      = false;
    s_notify_enabled = false;
    s_rx_len         = 0;

    OPERATE_RET rt = tal_ble_bt_init(TAL_BLE_ROLE_PERIPERAL, __ble_event_cb);
    if (rt != OPRT_OK) {
        PR_ERR("buddy_ble: tal_ble_bt_init failed rt=%d", rt);
        return rt;
    }

    PR_NOTICE("buddy_ble: init ok — waiting for stack ready event");
    return OPRT_OK;
}

OPERATE_RET buddy_ble_send_event(const char *event_type, const char *action_id)
{
    if (!event_type) {
        return OPRT_INVALID_PARM;
    }
    if (!s_connected || !s_notify_enabled) {
        return OPRT_RESOURCE_NOT_READY;
    }

    char buf[160];
    int n = snprintf(buf, sizeof(buf),
                     "{\"type\":\"event\",\"event\":\"%s\","
                     "\"action_id\":\"%s\"}\n",
                     event_type, action_id ? action_id : "");
    if (n <= 0 || (size_t)n >= sizeof(buf)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    TAL_BLE_DATA_T pkt = {
        .p_data = (uint8_t *)buf,
        .len    = (uint16_t)n,
    };
    OPERATE_RET rt = tal_ble_server_common_send(&pkt);
    if (rt != OPRT_OK) {
        PR_WARN("buddy_ble: send_event failed rt=%d", rt);
    }
    return rt;
}

bool buddy_ble_is_connected(void)
{
    return s_connected;
}

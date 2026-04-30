/**
 * @file buddy_http.c
 * @brief HTTP polling client: GET /api/state, POST /api/event.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "buddy_http.h"
#include "http_client_interface.h"
#include "tal_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************
 * Internal state
 ***********************************************************/
static char     s_host[64]  = {0};
static uint16_t s_port      = 8765;
static char     s_base_url[128] = {0};

/* Map bridge state string → buddy_state_e */
static buddy_state_e parse_state_str(const char *s)
{
    if (!s)                         return BUDDY_STATE_IDLE;
    if (strcmp(s, "IDLE")        == 0) return BUDDY_STATE_IDLE;
    if (strcmp(s, "BUSY")        == 0) return BUDDY_STATE_BUSY;
    if (strcmp(s, "ATTENTION")   == 0) return BUDDY_STATE_ATTENTION;
    if (strcmp(s, "CELEBRATE")   == 0) return BUDDY_STATE_CELEBRATE;
    if (strcmp(s, "DIZZY")       == 0) return BUDDY_STATE_DIZZY;
    if (strcmp(s, "DISCONNECTED")== 0) return BUDDY_STATE_DISCONNECTED;
    return BUDDY_STATE_IDLE;
}

/***********************************************************
 * Public API
 ***********************************************************/

OPERATE_RET buddy_http_init(const char *host, uint16_t port)
{
    if (!host) return OPRT_INVALID_PARM;
    strncpy(s_host, host, sizeof(s_host) - 1);
    s_port = port;
    snprintf(s_base_url, sizeof(s_base_url), "http://%s:%d", s_host, s_port);
    PR_NOTICE("buddy_http: bridge = %s", s_base_url);
    return OPRT_OK;
}

OPERATE_RET buddy_http_poll_state(buddy_state_info_t *info)
{
    if (!info) return OPRT_INVALID_PARM;

    http_client_response_t resp = {0};
    http_client_request_t req = {
        .host        = s_host,
        .port        = s_port,
        .path        = "/api/state",
        .method      = "GET",
        .timeout_ms  = 2000,
        .tls_no_verify = true,
    };

    http_client_status_t st = http_client_request(&req, &resp);
    if (st != HTTP_CLIENT_SUCCESS || resp.status_code != 200) {
        PR_DEBUG("buddy_http: GET /api/state failed status=%d http=%d",
                 st, resp.status_code);
        http_client_free(&resp);
        return OPRT_COM_ERROR;
    }

    /* Parse JSON */
    cJSON *root = cJSON_ParseWithLength((const char *)resp.body, resp.body_length);
    http_client_free(&resp);

    if (!root) {
        PR_ERR("buddy_http: JSON parse error");
        return OPRT_COM_ERROR;
    }

    cJSON *j;

    j = cJSON_GetObjectItem(root, "state");
    info->state = parse_state_str(cJSON_IsString(j) ? j->valuestring : NULL);

    j = cJSON_GetObjectItem(root, "sessions");
    info->session_count = cJSON_IsNumber(j) ? (uint32_t)j->valueint : 0;

    j = cJSON_GetObjectItem(root, "tokens");
    info->token_count = cJSON_IsNumber(j) ? (uint32_t)j->valueint : 0;

    j = cJSON_GetObjectItem(root, "message");
    if (cJSON_IsString(j)) {
        strncpy(info->message, j->valuestring, sizeof(info->message) - 1);
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    info->action.valid = false;
    if (cJSON_IsObject(action)) {
        j = cJSON_GetObjectItem(action, "id");
        if (cJSON_IsString(j) && j->valuestring[0] != '\0') {
            strncpy(info->action.id, j->valuestring, sizeof(info->action.id) - 1);
            j = cJSON_GetObjectItem(action, "prompt");
            if (cJSON_IsString(j)) {
                strncpy(info->action.prompt, j->valuestring, sizeof(info->action.prompt) - 1);
            }
            info->action.valid = true;
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

OPERATE_RET buddy_http_post_event(const char *event_type, const char *action_id)
{
    if (!event_type) return OPRT_INVALID_PARM;

    /* Build JSON body */
    char body[128];
    snprintf(body, sizeof(body),
             "{\"type\":\"%s\",\"action_id\":\"%s\"}",
             event_type, action_id ? action_id : "");

    http_client_header_t hdrs[] = {
        {"Content-Type", "application/json"},
    };
    http_client_response_t resp = {0};
    http_client_request_t req = {
        .host           = s_host,
        .port           = s_port,
        .path           = "/api/event",
        .method         = "POST",
        .headers        = hdrs,
        .headers_count  = 1,
        .body           = (const uint8_t *)body,
        .body_length    = strlen(body),
        .timeout_ms     = 2000,
        .tls_no_verify  = true,
    };

    http_client_status_t st = http_client_request(&req, &resp);
    http_client_free(&resp);

    if (st != HTTP_CLIENT_SUCCESS) {
        PR_WARN("buddy_http: POST /api/event failed: %d", st);
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("buddy_http: event '%s' sent (action_id=%s)", event_type,
              action_id ? action_id : "-");
    return OPRT_OK;
}

/**
 * @file buddy_ws.c
 * @brief WebSocket client for bridge connection (plain TCP, no TLS).
 *
 * Architecture (adapted from apps/DuckyClaw-UI/gateway/acp_client.c):
 *   - Raw TCP socket via tal_net_* (bypasses the TLS-forcing HTTP client lib)
 *   - RFC 6455 HTTP upgrade handshake sent manually
 *   - All frames sent client→server MUST be masked (RFC 6455 §5.1)
 *   - Background task: connect → upgrade → recv loop; auto-reconnect on error
 *   - On state_update message → invoke caller's buddy_ws_state_cb_t
 *   - buddy_ws_send_event() serialises with tx_mutex, sends a masked text frame
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "buddy_ws.h"
#include "cJSON.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_network.h"
#include "tal_system.h"
#include "tal_thread.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── WebSocket opcodes ─────────────────────────────────────────────────────*/
#define WS_OP_TEXT    0x01
#define WS_OP_CLOSE   0x08
#define WS_OP_PING    0x09
#define WS_OP_PONG    0x0A

/* ─── Tuning constants ──────────────────────────────────────────────────────*/
#define BWS_RX_BUF_SIZE     4096
#define BWS_RECONNECT_MS    5000
#define BWS_SELECT_MS       200
#define BWS_UPGRADE_TIMEOUT_MS 8000

/* If no data arrives for this many ms, assume the server is gone and reconnect.
 * The server sends a keepalive ping every 20 s, so 60 s gives 3 missed pings. */
#define BWS_IDLE_TIMEOUT_MS  60000u
#define BWS_IDLE_MAX_TICKS   (BWS_IDLE_TIMEOUT_MS / BWS_SELECT_MS)

/* ─── Connection state ──────────────────────────────────────────────────────*/
typedef enum {
    BWS_STATE_DISCONNECTED = 0,
    BWS_STATE_CONNECTED,
} bws_conn_e;

typedef struct {
    int            fd;
    bws_conn_e     conn;
    uint8_t        rx_buf[BWS_RX_BUF_SIZE];
    size_t         rx_len;
    MUTEX_HANDLE   tx_mutex;
    THREAD_HANDLE  thread;
    volatile bool  stop;
    char           host[64];
    uint16_t       port;
    uint32_t       idle_ticks;  /* consecutive select-timeout ticks */
} bws_ctx_t;

static bws_ctx_t           s_ctx;
static buddy_ws_state_cb_t s_state_cb = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 * Low-level TCP I/O  (identical pattern to acp_client.c)
 * ═══════════════════════════════════════════════════════════════════════════*/

static OPERATE_RET __send_all(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = tal_net_send(fd, buf + sent, (uint32_t)(len - sent));
        if (n == OPRT_RESOURCE_NOT_READY) {
            tal_system_sleep(5);
            continue;
        }
        if (n <= 0) {
            return OPRT_SEND_ERR;
        }
        sent += (size_t)n;
    }
    return OPRT_OK;
}

/**
 * @brief Receive up to len bytes within timeout_ms using select().
 * @return Bytes received (>0), 0 on timeout, -1 on error.
 */
static int __recv_timeout(int fd, uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        TUYA_FD_SET_T rfds;
        TAL_FD_ZERO(&rfds);
        TAL_FD_SET(fd, &rfds);
        int ready = tal_net_select(fd + 1, &rfds, NULL, NULL, 50);
        if (ready > 0 && TAL_FD_ISSET(fd, &rfds)) {
            int n = tal_net_recv(fd, buf, (uint32_t)len);
            if (n > 0) return n;
            if (n != OPRT_RESOURCE_NOT_READY) return -1;
        }
        elapsed += 50;
    }
    return 0; /* timeout */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WebSocket frame send (client must mask — RFC 6455 §5.1)
 * ═══════════════════════════════════════════════════════════════════════════*/

/** Generate a 4-byte masking key from system tick + monotonic counter. */
static void __gen_mask(uint8_t mask[4])
{
    static uint32_t s_ctr = 0;
    uint32_t v = tal_system_get_millisecond() ^ (++s_ctr * 0x9e3779b9u);
    mask[0] = (uint8_t)((v >> 24) & 0xFF);
    mask[1] = (uint8_t)((v >> 16) & 0xFF);
    mask[2] = (uint8_t)((v >>  8) & 0xFF);
    mask[3] = (uint8_t)( v        & 0xFF);
}

/**
 * @brief Build and send a masked WebSocket frame.
 * @param fd       Socket fd.
 * @param opcode   WS opcode (WS_OP_TEXT, WS_OP_PONG, …).
 * @param payload  Frame payload (may be NULL if plen == 0).
 * @param plen     Payload length in bytes.
 */
static OPERATE_RET __ws_send_masked(int fd, uint8_t opcode,
                                    const uint8_t *payload, size_t plen)
{
    uint8_t header[14] = {0};
    size_t  hlen;
    uint8_t mask[4];

    __gen_mask(mask);

    header[0] = (uint8_t)(0x80 | (opcode & 0x0F)); /* FIN=1 */

    if (plen <= 125) {
        header[1] = (uint8_t)(0x80 | plen);
        hlen = 2;
    } else if (plen <= 0xFFFF) {
        header[1] = (uint8_t)(0x80 | 126);
        header[2] = (uint8_t)((plen >> 8) & 0xFF);
        header[3] = (uint8_t)( plen       & 0xFF);
        hlen = 4;
    } else {
        /* Messages >64 KB not needed for this protocol */
        return OPRT_INVALID_PARM;
    }
    memcpy(header + hlen, mask, 4);
    hlen += 4;

    OPERATE_RET rt = __send_all(fd, header, hlen);
    if (rt != OPRT_OK || plen == 0) {
        return rt;
    }

    uint8_t *masked = (uint8_t *)malloc(plen);
    if (!masked) {
        return OPRT_MALLOC_FAILED;
    }
    for (size_t i = 0; i < plen; i++) {
        masked[i] = (uint8_t)(payload[i] ^ mask[i % 4]);
    }
    rt = __send_all(fd, masked, plen);
    free(masked);
    return rt;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WebSocket frame decode (server→client, unmasked)
 * ═══════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Decode one WebSocket frame from the head of rx_buf.
 * @return OPRT_OK           Frame decoded; *consumed bytes should be removed.
 * @return OPRT_RESOURCE_NOT_READY  Incomplete frame — wait for more data.
 */
static OPERATE_RET __ws_decode_frame(const uint8_t *buf, size_t len,
                                     uint8_t *opcode,
                                     const uint8_t **payload, size_t *plen,
                                     size_t *consumed)
{
    if (len < 2) {
        return OPRT_RESOURCE_NOT_READY;
    }

    *opcode       = (uint8_t)(buf[0] & 0x0F);
    bool   masked = (buf[1] & 0x80) != 0;
    uint64_t pl   = (uint64_t)(buf[1] & 0x7F);
    size_t   off  = 2;

    if (pl == 126) {
        if (len < off + 2) return OPRT_RESOURCE_NOT_READY;
        pl = ((uint64_t)buf[off] << 8) | buf[off + 1];
        off += 2;
    } else if (pl == 127) {
        if (len < off + 8) return OPRT_RESOURCE_NOT_READY;
        pl = 0;
        for (int i = 0; i < 8; i++) pl = (pl << 8) | buf[off + i];
        off += 8;
    }

    if (masked) {
        /* Server should never mask, but skip the key if present */
        if (len < off + 4) return OPRT_RESOURCE_NOT_READY;
        off += 4;
    }

    /* Guard against oversized frames filling the rx buffer */
    if (pl > (uint64_t)(BWS_RX_BUF_SIZE - off)) {
        size_t full = off + (size_t)pl;
        if (len < full) return OPRT_RESOURCE_NOT_READY;
        *consumed = full;
        *plen     = 0;
        *payload  = NULL;
        return OPRT_OK;
    }

    size_t frame = off + (size_t)pl;
    if (len < frame) {
        return OPRT_RESOURCE_NOT_READY;
    }

    *payload  = buf + off;
    *plen     = (size_t)pl;
    *consumed = frame;
    return OPRT_OK;
}

static void __rx_consume(size_t n)
{
    if (n == 0 || n > s_ctx.rx_len) return;
    if (n < s_ctx.rx_len) {
        memmove(s_ctx.rx_buf, s_ctx.rx_buf + n, s_ctx.rx_len - n);
    }
    s_ctx.rx_len -= n;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HTTP upgrade handshake
 * ═══════════════════════════════════════════════════════════════════════════*/

static OPERATE_RET __ws_upgrade(int fd)
{
    char req[512] = {0};
    int n = snprintf(req, sizeof(req),
        "GET /device HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Origin: http://%s:%u\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        s_ctx.host, (unsigned)s_ctx.port,
        s_ctx.host, (unsigned)s_ctx.port);
    if (n <= 0 || (size_t)n >= sizeof(req)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    OPERATE_RET rt = __send_all(fd, (const uint8_t *)req, (size_t)n);
    if (rt != OPRT_OK) {
        PR_ERR("buddy_ws: upgrade send failed rt=%d", rt);
        return rt;
    }

    uint8_t resp[512] = {0};
    int got = __recv_timeout(fd, resp, sizeof(resp) - 1, BWS_UPGRADE_TIMEOUT_MS);
    if (got <= 0) {
        PR_ERR("buddy_ws: upgrade recv timeout/err got=%d", got);
        return OPRT_RECV_ERR;
    }
    resp[got] = '\0';

    if (!strstr((const char *)resp, "101")) {
        PR_ERR("buddy_ws: upgrade rejected: %.80s", (const char *)resp);
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("buddy_ws: HTTP upgrade ok");
    return OPRT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * JSON message dispatch
 * ═══════════════════════════════════════════════════════════════════════════*/

static buddy_state_e __parse_state_str(const char *s)
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

static void __dispatch_msg(const uint8_t *payload, size_t plen)
{
    if (!payload || plen == 0 || !s_state_cb) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)payload, plen);
    if (!root) {
        PR_WARN("buddy_ws: JSON parse error");
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
        info.state = __parse_state_str(cJSON_IsString(j) ? j->valuestring : NULL);

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
                strncpy(info.action.id, j->valuestring, sizeof(info.action.id) - 1);
                cJSON *p = cJSON_GetObjectItem(action, "prompt");
                if (cJSON_IsString(p)) {
                    strncpy(info.action.prompt, p->valuestring, sizeof(info.action.prompt) - 1);
                }
                info.action.valid = true;
            }
        }

        s_state_cb(&info);
    }

    cJSON_Delete(root);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Connection lifecycle
 * ═══════════════════════════════════════════════════════════════════════════*/

static OPERATE_RET __connect(void)
{
    /* Resolve host */
    TUYA_IP_ADDR_T ip = tal_net_str2addr(s_ctx.host);
    if (ip == 0) {
        OPERATE_RET rt = tal_net_gethostbyname(s_ctx.host, &ip);
        if (rt != OPRT_OK || ip == 0) {
            PR_ERR("buddy_ws: DNS resolve failed host=%s", s_ctx.host);
            return OPRT_NETWORK_ERROR;
        }
    }

    int fd = tal_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        PR_ERR("buddy_ws: socket create failed");
        return OPRT_SOCK_ERR;
    }

    OPERATE_RET rt = tal_net_connect(fd, ip, s_ctx.port);
    if (rt != OPRT_OK) {
        PR_WARN("buddy_ws: TCP connect failed rt=%d host=%s:%u",
                rt, s_ctx.host, (unsigned)s_ctx.port);
        tal_net_close(fd);
        return OPRT_NETWORK_ERROR;
    }

    rt = __ws_upgrade(fd);
    if (rt != OPRT_OK) {
        tal_net_close(fd);
        return rt;
    }

    s_ctx.fd         = fd;
    s_ctx.conn       = BWS_STATE_CONNECTED;
    s_ctx.rx_len     = 0;
    s_ctx.idle_ticks = 0;
    PR_NOTICE("buddy_ws: connected to %s:%u", s_ctx.host, (unsigned)s_ctx.port);
    return OPRT_OK;
}

static void __disconnect(void)
{
    if (s_ctx.fd >= 0) {
        tal_net_close(s_ctx.fd);
        s_ctx.fd = -1;
    }
    s_ctx.conn   = BWS_STATE_DISCONNECTED;
    s_ctx.rx_len = 0;

    /* Notify caller that bridge is offline */
    if (s_state_cb) {
        buddy_state_info_t dc = {0};
        dc.state = BUDDY_STATE_DISCONNECTED;
        s_state_cb(&dc);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Background task
 * ═══════════════════════════════════════════════════════════════════════════*/

static void __bws_task(void *arg)
{
    (void)arg;
    PR_NOTICE("buddy_ws: task started");

    while (!s_ctx.stop) {

        /* ── Connect phase ── */
        if (s_ctx.conn == BWS_STATE_DISCONNECTED) {
            if (__connect() != OPRT_OK) {
                tal_system_sleep(BWS_RECONNECT_MS);
                continue;
            }
        }

        /* ── Receive one chunk ── */
        TUYA_FD_SET_T rfds;
        TAL_FD_ZERO(&rfds);
        TAL_FD_SET(s_ctx.fd, &rfds);
        int ready = tal_net_select(s_ctx.fd + 1, &rfds, NULL, NULL, BWS_SELECT_MS);
        if (ready <= 0) {
            /* Track idle time; reconnect if server goes silent for too long */
            s_ctx.idle_ticks++;
            if (s_ctx.idle_ticks >= BWS_IDLE_MAX_TICKS) {
                PR_WARN("buddy_ws: idle timeout (%u ms), reconnecting",
                        (unsigned)BWS_IDLE_TIMEOUT_MS);
                s_ctx.idle_ticks = 0;
                __disconnect();
            }
            continue;
        }
        if (!TAL_FD_ISSET(s_ctx.fd, &rfds)) {
            continue;
        }
        s_ctx.idle_ticks = 0;   /* data or event received — connection is alive */

        if (s_ctx.rx_len >= BWS_RX_BUF_SIZE) {
            PR_WARN("buddy_ws: rx buffer full, reconnecting");
            __disconnect();
            continue;
        }

        int n = tal_net_recv(s_ctx.fd,
                             s_ctx.rx_buf + s_ctx.rx_len,
                             (uint32_t)(BWS_RX_BUF_SIZE - s_ctx.rx_len));
        if (n == OPRT_RESOURCE_NOT_READY) {
            continue;
        }
        if (n <= 0) {
            PR_WARN("buddy_ws: connection closed by server");
            __disconnect();
            continue;
        }
        s_ctx.rx_len += (size_t)n;

        /* ── Frame decode loop ── */
        while (s_ctx.rx_len > 0 && s_ctx.conn == BWS_STATE_CONNECTED) {
            uint8_t        opcode   = 0;
            const uint8_t *payload  = NULL;
            size_t         plen     = 0;
            size_t         consumed = 0;

            OPERATE_RET rt = __ws_decode_frame(s_ctx.rx_buf, s_ctx.rx_len,
                                               &opcode, &payload, &plen, &consumed);
            if (rt == OPRT_RESOURCE_NOT_READY) {
                break; /* wait for more data */
            }
            if (rt != OPRT_OK) {
                PR_WARN("buddy_ws: frame decode error rt=%d", rt);
                __disconnect();
                break;
            }

            switch (opcode) {
            case WS_OP_TEXT:
                if (payload && plen > 0) {
                    __dispatch_msg(payload, plen);
                }
                break;
            case WS_OP_PING:
                /* Respond with PONG — payload is echoed back */
                tal_mutex_lock(s_ctx.tx_mutex);
                (void)__ws_send_masked(s_ctx.fd, WS_OP_PONG, payload, plen);
                tal_mutex_unlock(s_ctx.tx_mutex);
                break;
            case WS_OP_CLOSE:
                PR_NOTICE("buddy_ws: server sent close frame");
                __disconnect();
                break;
            default:
                break;
            }

            __rx_consume(consumed);
        }
    }

    PR_NOTICE("buddy_ws: task stopped");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════*/

OPERATE_RET buddy_ws_init(const char *host, uint16_t port, buddy_ws_state_cb_t cb)
{
    if (!host || !cb) {
        return OPRT_INVALID_PARM;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.fd   = -1;
    s_ctx.conn = BWS_STATE_DISCONNECTED;
    strncpy(s_ctx.host, host, sizeof(s_ctx.host) - 1);
    s_ctx.port = port;
    s_state_cb = cb;

    OPERATE_RET rt = tal_mutex_create_init(&s_ctx.tx_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("buddy_ws: mutex create failed rt=%d", rt);
        return rt;
    }

    THREAD_CFG_T tcfg = {
        .stackDepth = 1024 * 6,
        .priority   = THREAD_PRIO_2,
        .thrdname   = "buddy_ws",
    };
    s_ctx.stop = false;
    rt = tal_thread_create_and_start(&s_ctx.thread, NULL, NULL,
                                     __bws_task, NULL, &tcfg);
    if (rt != OPRT_OK) {
        PR_ERR("buddy_ws: thread create failed rt=%d", rt);
        return rt;
    }

    PR_NOTICE("buddy_ws: init ok host=%s port=%u", host, (unsigned)port);
    return OPRT_OK;
}

OPERATE_RET buddy_ws_send_event(const char *event_type, const char *action_id)
{
    if (!event_type) {
        return OPRT_INVALID_PARM;
    }
    if (s_ctx.conn != BWS_STATE_CONNECTED || s_ctx.fd < 0) {
        return OPRT_RESOURCE_NOT_READY;
    }

    char buf[128] = {0};
    snprintf(buf, sizeof(buf),
             "{\"type\":\"event\",\"event\":\"%s\",\"action_id\":\"%s\"}",
             event_type, action_id ? action_id : "");

    tal_mutex_lock(s_ctx.tx_mutex);
    OPERATE_RET rt = __ws_send_masked(s_ctx.fd, WS_OP_TEXT,
                                      (const uint8_t *)buf, strlen(buf));
    tal_mutex_unlock(s_ctx.tx_mutex);

    if (rt != OPRT_OK) {
        PR_WARN("buddy_ws: send_event failed rt=%d", rt);
    }
    return rt;
}

bool buddy_ws_is_connected(void)
{
    return (s_ctx.conn == BWS_STATE_CONNECTED);
}

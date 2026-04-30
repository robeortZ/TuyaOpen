/**
 * @file buddy_config.c
 * @brief SoftAP configuration server for first-run / re-configuration.
 *
 * Flow:
 *   1. Set Wi-Fi mode to SOFTAP and start AP "BuddyPixel-Setup" (open, ch 6).
 *   2. Set AP IP to 192.168.4.1.
 *   3. Listen on TCP port 80; serve a single HTML config page.
 *   4. On form POST: parse URL-encoded fields (ssid / pass / host / port),
 *      save to NVS via tal_kv_set, send success page, reset after 2 s.
 *   5. On 5-minute idle timeout: return without resetting (fallback).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "buddy_config.h"
#include "tal_kv.h"
#include "tal_wifi.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_network.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/***********************************************************
 * NVS keys
 ***********************************************************/
#define KV_SSID  "bp_ssid"
#define KV_PASS  "bp_pass"
#define KV_HOST  "bp_host"
#define KV_PORT  "bp_port"

/***********************************************************
 * SoftAP parameters
 ***********************************************************/
#define AP_SSID     "BuddyPixel-Setup"
#define AP_IP       "192.168.4.1"
#define AP_PORT     80
#define AP_TIMEOUT_MS  300000u   /* 5 minutes */

/***********************************************************
 * Embedded HTML pages
 ***********************************************************/
static const char s_html_config[] =
    "<!DOCTYPE html><html lang=\"zh-CN\">"
    "<head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Buddy Pixel \xe9\x85\x8d\xe7\xbd\xae</title>"  /* 配置 */
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font:14px/1.6 sans-serif;background:#0d1117;color:#e6edf3;"
         "max-width:440px;margin:40px auto;padding:20px}"
    "h1{color:#58a6ff;margin-bottom:6px;font-size:20px}"
    "p{color:#8b949e;font-size:12px;margin-bottom:20px}"
    ".card{background:#161b22;border:1px solid #30363d;border-radius:8px;"
          "padding:16px;margin-bottom:12px}"
    ".card h2{font-size:11px;color:#8b949e;text-transform:uppercase;"
              "letter-spacing:.08em;margin-bottom:12px}"
    "label{display:block;font-size:12px;color:#8b949e;margin-top:10px;"
           "margin-bottom:3px}"
    "input{width:100%;padding:8px 10px;border:1px solid #30363d;"
           "border-radius:6px;background:#0d1117;color:#e6edf3;font-size:15px}"
    "input:focus{outline:none;border-color:#58a6ff}"
    "button{width:100%;margin-top:20px;padding:12px;background:#238636;"
            "color:#fff;border:none;border-radius:6px;font-size:15px;"
            "cursor:pointer}"
    "button:hover{background:#2ea043}"
    ".hint{font-size:11px;color:#555;margin-top:4px}"
    "</style></head><body>"
    "<h1>&#129302; Buddy Pixel \xe9\x85\x8d\xe7\xbd\xae</h1>"
    "<p>\xe8\xbf\x9e\xe6\x8e\xa5 WiFi: <b>BuddyPixel-Setup</b> "
       "\xe5\x90\x8e\xe8\xae\xbf\xe9\x97\xae http://192.168.4.1</p>"
    "<form method=\"POST\" action=\"/\">"
    "<div class=\"card\">"
    "<h2>&#128246; WiFi \xe7\xbd\x91\xe7\xbb\x9c</h2>"
    "<label>SSID (\xe7\xbd\x91\xe7\xbb\x9c\xe5\x90\x8d\xe7\xa7\xb0)</label>"
    "<input type=\"text\" name=\"ssid\" placeholder=\"Your WiFi SSID\" required>"
    "<label>\xe5\xaf\x86\xe7\xa0\x81 (Password)</label>"
    "<input type=\"password\" name=\"pass\" "
           "placeholder=\"\xe7\xa9\xba\xe6\xa0\xbc\xe8\xa1\xa8\xe7\xa4\xba\xe6\x97\xa0\xe5\x8a\xa0\xe5\xaf\x86\">"
    "</div>"
    "<div class=\"card\">"
    "<h2>&#128432; \xe6\xa1\xa5\xe6\x8e\xa5\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8</h2>"
    "<label>IP \xe5\x9c\xb0\xe5\x9d\x80</label>"
    "<input type=\"text\" name=\"host\" placeholder=\"192.168.1.100\" required>"
    "<p class=\"hint\">\xe8\xbf\x90\xe8\xa1\x8c buddy_bridge \xe7\x9a\x84\xe7\x94\xb5\xe8\x84\x91 IP</p>"
    "<label>\xe7\xab\xaf\xe5\x8f\xa3</label>"
    "<input type=\"number\" name=\"port\" value=\"8765\" min=\"1\" max=\"65535\">"
    "</div>"
    "<button type=\"submit\">&#128190; "
    "\xe4\xbf\x9d\xe5\xad\x98\xe9\x85\x8d\xe7\xbd\xae\xe5\xb9\xb6\xe9\x87\x8d\xe5\x90\xaf"
    "</button>"
    "</form></body></html>";

static const char s_html_ok[] =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>OK</title></head>"
    "<body style=\"font:14px sans-serif;background:#0d1117;color:#e6edf3;"
              "max-width:440px;margin:60px auto;padding:20px;text-align:center\">"
    "<div style=\"font-size:56px\">&#9989;</div>"
    "<h2 style=\"color:#3fb950;margin:16px 0 8px\">"
    "\xe9\x85\x8d\xe7\xbd\xae\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98\xef\xbc\x81</h2>"
    "<p style=\"color:#8b949e\">"
    "\xe8\xae\xbe\xe5\xa4\x87\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe5\x90\xaf\xe2\x80\xa6<br>"
    "\xe8\xaf\xb7\xe9\x87\x8d\xe6\x96\xb0\xe8\xbf\x9e\xe6\x8e\xa5\xe5\x8e\x9f WiFi \xe7\xbd\x91\xe7\xbb\x9c\xe3\x80\x82"
    "</p></body></html>";

/***********************************************************
 * URL decode (application/x-www-form-urlencoded)
 ***********************************************************/
static void __url_decode(char *dst, const char *src, size_t dlen)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dlen - 1; si++) {
        if (src[si] == '+') {
            dst[di++] = ' ';
        } else if (src[si] == '%' && src[si+1] && src[si+2]) {
            char hex[3] = { src[si+1], src[si+2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

/***********************************************************
 * Parse URL-encoded form body into config
 ***********************************************************/
static void __parse_form(const char *body, buddy_config_t *cfg)
{
    char tmp[512] = {0};
    strncpy(tmp, body, sizeof(tmp) - 1);

    char *tok = tmp;
    while (tok) {
        char *next = strchr(tok, '&');
        if (next) *next++ = '\0';

        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            char decoded[128] = {0};
            __url_decode(decoded, eq + 1, sizeof(decoded));

            if (strcmp(tok, "ssid") == 0)
                strncpy(cfg->ssid, decoded, BUDDY_CFG_SSID_LEN);
            else if (strcmp(tok, "pass") == 0)
                strncpy(cfg->pass, decoded, BUDDY_CFG_PASS_LEN);
            else if (strcmp(tok, "host") == 0)
                strncpy(cfg->host, decoded, BUDDY_CFG_HOST_LEN);
            else if (strcmp(tok, "port") == 0)
                cfg->port = (uint16_t)atoi(decoded);
        }
        tok = next;
    }
}

/***********************************************************
 * Send a complete HTTP 200 response
 ***********************************************************/
static void __http_send(int fd, const char *html, size_t hlen)
{
    char hdr[128];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        (unsigned)hlen);
    if (n > 0) {
        tal_net_send(fd, (const uint8_t *)hdr, (uint32_t)n);
        tal_net_send(fd, (const uint8_t *)html, (uint32_t)hlen);
    }
}

/***********************************************************
 * Minimal HTTP server — one connection at a time
 * Returns true if a valid config was received.
 ***********************************************************/
static bool __run_http_server(buddy_config_t *cfg)
{
    int srv = tal_net_socket_create(PROTOCOL_TCP);
    if (srv < 0) {
        PR_ERR("buddy_cfg: socket create failed");
        return false;
    }

    if (tal_net_bind(srv, 0, AP_PORT) != OPRT_OK) {
        PR_ERR("buddy_cfg: bind port %d failed", AP_PORT);
        tal_net_close(srv);
        return false;
    }
    tal_net_listen(srv, 2);
    PR_NOTICE("buddy_cfg: HTTP server listening on :%d", AP_PORT);

    /* Allocate the receive buffer on the heap to avoid blowing the stack
     * (2 KB on the stack of tuya_app would overflow a 4 KB thread stack). */
    char *rx = (char *)malloc(2048);
    if (!rx) {
        PR_ERR("buddy_cfg: rx malloc failed");
        tal_net_close(srv);
        return false;
    }

    bool got_cfg     = false;
    uint32_t start   = tal_system_get_millisecond();

    while (!got_cfg &&
           (tal_system_get_millisecond() - start) < AP_TIMEOUT_MS) {

        /* Accept with 500 ms select timeout */
        TUYA_FD_SET_T rfds;
        TAL_FD_ZERO(&rfds);
        TAL_FD_SET(srv, &rfds);
        if (tal_net_select(srv + 1, &rfds, NULL, NULL, 500) <= 0)
            continue;

        TUYA_IP_ADDR_T cli_ip = 0;
        uint16_t       cli_port = 0;
        int cli = tal_net_accept(srv, &cli_ip, &cli_port);
        if (cli < 0) continue;

        /* Read request (up to 2 KB, 2 s timeout) */
        memset(rx, 0, 2048);
        uint32_t rx_len = 0;
        uint32_t t0 = tal_system_get_millisecond();
        while (rx_len < 2047 &&
               (tal_system_get_millisecond() - t0) < 2000) {
            TUYA_FD_SET_T rfds2;
            TAL_FD_ZERO(&rfds2);
            TAL_FD_SET(cli, &rfds2);
            if (tal_net_select(cli + 1, &rfds2, NULL, NULL, 100) <= 0) break;
            int n = tal_net_recv(cli, (uint8_t *)(rx + rx_len),
                                 (uint32_t)(2047 - rx_len));
            if (n <= 0) break;
            rx_len += (uint32_t)n;
            /* Stop reading once we have the full headers */
            if (strstr(rx, "\r\n\r\n")) break;
        }

        if (rx_len == 0) { tal_net_close(cli); continue; }

        if (strncmp(rx, "POST", 4) == 0) {
            /* Body starts after the blank line */
            char *body = strstr(rx, "\r\n\r\n");
            if (body) {
                body += 4;
                memset(cfg, 0, sizeof(*cfg));
                cfg->port = 8765;
                __parse_form(body, cfg);
                if (cfg->ssid[0] != '\0' && cfg->host[0] != '\0') {
                    __http_send(cli, s_html_ok, sizeof(s_html_ok) - 1);
                    got_cfg = true;
                    PR_NOTICE("buddy_cfg: config received ssid=%s host=%s:%u",
                              cfg->ssid, cfg->host, (unsigned)cfg->port);
                } else {
                    __http_send(cli, s_html_config, sizeof(s_html_config) - 1);
                }
            }
        } else {
            /* All other requests (GET, favicon, etc.) → serve config page */
            __http_send(cli, s_html_config, sizeof(s_html_config) - 1);
        }

        tal_net_close(cli);
    }

    free(rx);
    tal_net_close(srv);
    return got_cfg;
}

/***********************************************************
 * Public API
 ***********************************************************/

bool buddy_config_load(buddy_config_t *cfg)
{
    if (!cfg) return false;
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = 8765;

    uint8_t *val = NULL;
    size_t   len = 0;

    if (tal_kv_get(KV_SSID, &val, &len) != OPRT_OK || len == 0) return false;
    strncpy(cfg->ssid, (char *)val, BUDDY_CFG_SSID_LEN);
    tal_kv_free(val); val = NULL;

    if (tal_kv_get(KV_HOST, &val, &len) != OPRT_OK || len == 0) return false;
    strncpy(cfg->host, (char *)val, BUDDY_CFG_HOST_LEN);
    tal_kv_free(val); val = NULL;

    if (tal_kv_get(KV_PASS, &val, &len) == OPRT_OK && len > 0) {
        strncpy(cfg->pass, (char *)val, BUDDY_CFG_PASS_LEN);
        tal_kv_free(val); val = NULL;
    }

    if (tal_kv_get(KV_PORT, &val, &len) == OPRT_OK && len > 0) {
        cfg->port = (uint16_t)atoi((char *)val);
        tal_kv_free(val); val = NULL;
    }

    return (cfg->ssid[0] != '\0' && cfg->host[0] != '\0');
}

OPERATE_RET buddy_config_save(const buddy_config_t *cfg)
{
    if (!cfg) return OPRT_INVALID_PARM;

    char port_str[8] = {0};
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)cfg->port);

    OPERATE_RET rt;
    rt = tal_kv_set(KV_SSID, (uint8_t *)cfg->ssid, strlen(cfg->ssid) + 1);
    if (rt != OPRT_OK) return rt;
    rt = tal_kv_set(KV_PASS, (uint8_t *)cfg->pass, strlen(cfg->pass) + 1);
    if (rt != OPRT_OK) return rt;
    rt = tal_kv_set(KV_HOST, (uint8_t *)cfg->host, strlen(cfg->host) + 1);
    if (rt != OPRT_OK) return rt;
    rt = tal_kv_set(KV_PORT, (uint8_t *)port_str, strlen(port_str) + 1);
    return rt;
}

void buddy_config_clear(void)
{
    tal_kv_del(KV_SSID);
    tal_kv_del(KV_PASS);
    tal_kv_del(KV_HOST);
    tal_kv_del(KV_PORT);
    PR_NOTICE("buddy_cfg: config cleared");
}

void buddy_config_run_server(void)
{
    PR_NOTICE("buddy_cfg: entering config mode (AP: %s, IP: %s)",
              AP_SSID, AP_IP);

    /* Set SoftAP mode */
    tal_wifi_set_work_mode(WWM_SOFTAP);

    /* Configure and start the AP */
    WF_AP_CFG_IF_S ap = {0};
    strncpy((char *)ap.ssid, AP_SSID, sizeof(ap.ssid) - 1);
    ap.s_len       = (uint8_t)strlen(AP_SSID);
    ap.chan        = 6;
    ap.md          = WAAM_OPEN;
    ap.ssid_hidden = 0;
    ap.max_conn    = 2;
    ap.ms_interval = 100;
    strncpy(ap.ip.ip,   AP_IP,           sizeof(ap.ip.ip)   - 1);
    strncpy(ap.ip.gw,   AP_IP,           sizeof(ap.ip.gw)   - 1);
    strncpy(ap.ip.mask, "255.255.255.0", sizeof(ap.ip.mask) - 1);

    OPERATE_RET rt = tal_wifi_ap_start(&ap);
    if (rt != OPRT_OK) {
        PR_ERR("buddy_cfg: tal_wifi_ap_start failed rt=%d", rt);
    }

    tal_system_sleep(1000); /* let AP + DHCP server settle */

    /* Run HTTP config server */
    buddy_config_t cfg = {0};
    bool saved = __run_http_server(&cfg);

    if (saved) {
        buddy_config_save(&cfg);
        PR_NOTICE("buddy_cfg: config saved — resetting in 2 s");
        tal_system_sleep(2000);
        tal_system_reset();
        /* does not return */
    } else {
        PR_WARN("buddy_cfg: config timeout — continuing with no config");
        tal_wifi_ap_stop();
    }
}

/**
 * @file buddy_config.h
 * @brief Run-time configuration stored in NVS, provisioned via SoftAP + HTTP.
 *
 * On first boot (no NVS config) the device starts a Wi-Fi access point called
 * "BuddyPixel-Setup" and serves a config web page at http://192.168.4.1.
 * The user fills in Wi-Fi credentials and the bridge server address, then the
 * config is saved to NVS and the device reboots into normal operation.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#pragma once

#include "tuya_cloud_types.h"
#include <stdbool.h>
#include <stdint.h>

#define BUDDY_CFG_SSID_LEN  32
#define BUDDY_CFG_PASS_LEN  64
#define BUDDY_CFG_HOST_LEN  64

typedef struct {
    char     ssid[BUDDY_CFG_SSID_LEN + 1];
    char     pass[BUDDY_CFG_PASS_LEN + 1];
    char     host[BUDDY_CFG_HOST_LEN + 1];
    uint16_t port;
} buddy_config_t;

/**
 * @brief Load config from NVS.
 * @return true if a valid config (non-empty SSID + host) was found.
 */
bool buddy_config_load(buddy_config_t *cfg);

/**
 * @brief Persist config to NVS.
 */
OPERATE_RET buddy_config_save(const buddy_config_t *cfg);

/**
 * @brief Wipe config from NVS (forces config-mode on next boot).
 */
void buddy_config_clear(void);

/**
 * @brief Start SoftAP "BuddyPixel-Setup" and serve the HTTP config page.
 *        Blocks until the user submits the form; saves to NVS then resets.
 *        If no submission arrives within 5 minutes, returns without resetting
 *        so the caller can fall through to offline operation.
 */
void buddy_config_run_server(void);

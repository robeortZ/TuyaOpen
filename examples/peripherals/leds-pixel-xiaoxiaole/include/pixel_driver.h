/**
 * @file pixel_driver.h
 * @brief Reusable WS2812 Pixel LED Driver Module
 *
 * This module provides a portable interface for WS2812 LED strip control
 * that can be used across different T5AI boards (PIXEL, BOARD, POCKET, etc.)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __PIXEL_DRIVER_H__
#define __PIXEL_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and register the Pixel LED (WS2812) driver
 *
 * This function initializes the WS2812 LED strip driver with the following settings:
 * - SPI Port: TUYA_SPI_NUM_1
 * - Color Order: RGB_ORDER
 * - Device Name: "pixel" (or custom name from PIXEL_DEVICE_NAME macro)
 *
 * Requirements:
 * - CONFIG_ENABLE_LEDS_PIXEL must be enabled
 * - CONFIG_ENABLE_SPI must be enabled
 * - SPI hardware must be configured
 *
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET pixel_driver_init(void);

#ifdef __cplusplus
}
#endif

#endif // __PIXEL_DRIVER_H__

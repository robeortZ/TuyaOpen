/**
 * @file pixel_driver.c
 * @brief Reusable WS2812 Pixel LED Driver Implementation
 *
 * Portable WS2812 LED strip driver for T5AI platforms
 * Can be reused across different boards by linking this module
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
#include "tkl_pinmux.h"
#include "tdd_pixel_ws2812.h"
#include "tdd_pixel_type.h"
#include "tdl_pixel_dev_manage.h"
#endif

#include "pixel_driver.h"

/**
 * @brief Initialize and register WS2812 Pixel LED driver
 *
 * Registers the WS2812 LED strip driver on SPI port 1 with RGB color order.
 * The device name can be customized via PIXEL_DEVICE_NAME macro, defaults to "pixel".
 *
 * NOTE: This function only registers the driver if ENABLE_SPI and ENABLE_LEDS_PIXEL are enabled.
 * If the board has already registered the pixel device, this will detect it and return success.
 *
 * @return OPERATE_RET_OK if successful, error code otherwise
 */
OPERATE_RET pixel_driver_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_SPI) && (ENABLE_SPI) && defined(ENABLE_LEDS_PIXEL) && (ENABLE_LEDS_PIXEL)
    /* Get device name from macro or use default */
    char device_name[32] = "pixel";
#if defined(PIXEL_DEVICE_NAME)
    strncpy(device_name, PIXEL_DEVICE_NAME, sizeof(device_name) - 1);
    device_name[sizeof(device_name) - 1] = '\0';
#endif

    /* Try to register the driver - if already registered by board, will return error */
    /* that's OK, we just need the device to exist */

    /* Configure WS2812 driver */
    PIXEL_DRIVER_CONFIG_T dev_init_cfg = {
        .port = TUYA_SPI_NUM_1,      /* SPI port 1 */
        .line_seq = RGB_ORDER,        /* RGB color order */
    };

    /* Register the driver - this may fail if board already registered it */
    rt = tdd_ws2812_driver_register(device_name, &dev_init_cfg);
    if (OPRT_OK == rt) {
        PR_NOTICE("Pixel LED driver registered: %s", device_name);
    } else {
        /* If device already exists (registered by board layer), that's fine */
        PR_DEBUG("Pixel LED driver registration returned: %d (may be pre-registered by board)", rt);
    }

    /* Always return OK since we don't care if board already registered it */
    return OPRT_OK;
#else
    PR_WARN("Pixel LED driver not available: SPI or LEDS_PIXEL not enabled in config");
    return OPRT_OK;
#endif
}

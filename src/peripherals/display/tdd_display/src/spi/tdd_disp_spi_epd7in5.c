/**
 * @file tdd_disp_spi_epd7in5.c
 * @brief ST7305 monochrome LCD driver implementation with SPI interface
 *
 * This file provides the implementation for ST7305 monochrome LCD displays using SPI interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for ST7305 displays. The ST7305 is designed for monochrome displays
 * commonly used in industrial and embedded applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_memory.h"

#include "tdd_display_spi.h"
#include "tdd_disp_epd7in5.h"
#include "__EPD_7in5_V2.h"

/***********************************************************
***********************MACRO define**********************
***********************************************************/
#define GET_ROUND_UP_TO_MULTI_OF_3(num) (((num) % 3 == 0) ? (num) : ((num) + (3 - (num) % 3)))

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    DISP_SPI_BASE_CFG_T cfg;
    TDL_DISP_FRAME_BUFF_T *convert_fb; // Frame buffer for conversion
} DISP_EPD7IN5_DEV_T;

/***********************************************************
***********************const define**********************
***********************************************************/
// static uint8_t EPD7IN5_INIT_SEQ[] = {
//     5,  0,   0x01, 0x17, 0x07,0x3f,0x3f,                                                 // NVM Load Control
//     5,  0,   0x06, 0x017,0x17,0x28,0x17,                                                       // Booster Enable
//     1,  100, 0x04,                                                 // Gate Voltage Setting
//     2,  0,   0x00, 0X1f,                                     // VSHP Setting (4.8V)
//     5,  0,   0x61, 0X03, 0X20, 0X01, 0Xe0,                                     // VSLP Setting (0.98V)
//     2,  0,   0x15, 0X00,                                    // VSHN Setting (-3.6V)
//     3,  0,   0x50, 0X10, 0X07,                                   // VSLN Setting (0.22V)
//     2,  0,   0x60, 0x22,                                                // OSC Setting
//     0                    // Terminate list
// };

/***********************************************************
***********************function define**********************
***********************************************************/
// Pixel data structure is as follows:
// P0 P2 P4 P6
// P1 P3 P5 P7

// Corresponds to one byte of data:
// BIT7 BIT5 BIT3 BIT1
// BIT6 BIT4 BIT2 BIT0
static void __tdd_epd7in5_convert(uint32_t width, uint32_t height, uint8_t *in_buf, uint8_t *out_buf)
{

}



static OPERATE_RET __tdd_disp_spi_epd7in5_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_EPD7IN5_DEV_T *disp_spi_dev = NULL;
    // uint8_t gate_line = 0;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }
    disp_spi_dev = (DISP_EPD7IN5_DEV_T *)device;


    DEV_Module_Init();
    tdd_disp_spi_init(&(disp_spi_dev->cfg));
    EPD_7IN5_V2_Init(&(disp_spi_dev->cfg));
    EPD_7IN5_V2_Clear();
    DEV_Delay_ms(500);
    // EPD_7IN5_V2_Init_4Gray(&(disp_spi_dev->cfg));
    // Paint_SetScale(4);
    // Paint_Clear(0xff);
    

    PR_DEBUG("[EPD7IN5] Initialize display device successful.");

    return OPRT_OK;
}

static OPERATE_RET __tdd_disp_spi_epd7in5_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_EPD7IN5_DEV_T *disp_spi_dev = NULL;

    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_spi_dev = (DISP_EPD7IN5_DEV_T *)device;

    __tdd_epd7in5_convert(disp_spi_dev->cfg.width, disp_spi_dev->cfg.height, frame_buff->frame,
                         disp_spi_dev->convert_fb->frame);

    tdd_disp_spi_send_data(&disp_spi_dev->cfg, disp_spi_dev->convert_fb->frame, disp_spi_dev->convert_fb->len);
    //Display the frame buffer
    EPD_7IN5_V2_Display_4Gray(frame_buff->frame);
    // EPD_7IN5_V2_Display(disp_spi_dev->convert_fb->frame);
    return rt;
}

static OPERATE_RET __tdd_disp_spi_epd7in5_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief Registers an ST7305 monochrome display device using the SPI interface with the display management system.
 *
 * This function creates and initializes a new ST7305 display device instance, 
 * configures its frame buffer and hardware-specific settings, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 * @param caset_xs Column address start value used in display window configuration.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
 OPERATE_RET tdd_disp_spi_epd7in5_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
 {
     OPERATE_RET rt = OPRT_OK;
     uint32_t frame_len = 0;
     DISP_EPD7IN5_DEV_T *disp_spi_dev = NULL;
     TDD_DISP_DEV_INFO_T disp_spi_dev_info;
 
     if (NULL == name || NULL == dev_cfg) {
         return OPRT_INVALID_PARM;
     }
 
     // Validate display dimensions
     if (dev_cfg->width != EPD_7IN5_V2_WIDTH || dev_cfg->height != EPD_7IN5_V2_HEIGHT) {
         PR_ERR("Invalid display dimensions. Expected %dx%d, got %dx%d", 
                EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, dev_cfg->width, dev_cfg->height);
         return OPRT_INVALID_PARM;
     }
 
     disp_spi_dev = (DISP_EPD7IN5_DEV_T *)tal_malloc(sizeof(DISP_EPD7IN5_DEV_T));
     if (NULL == disp_spi_dev) {
         return OPRT_MALLOC_FAILED;
     }
     memset(disp_spi_dev, 0x00, sizeof(DISP_EPD7IN5_DEV_T));
 
     // Calculate frame buffer size (4Gray: 2 bits per pixel)
     frame_len = (dev_cfg->width * dev_cfg->height) / 4;
     disp_spi_dev->convert_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
     if (NULL == disp_spi_dev->convert_fb) {
         tal_free(disp_spi_dev);
         return OPRT_MALLOC_FAILED;
     }
     memset(disp_spi_dev->convert_fb->frame, 0x00, frame_len);
     disp_spi_dev->convert_fb->fmt = TUYA_PIXEL_FMT_I2;
     disp_spi_dev->convert_fb->width = dev_cfg->width;
     disp_spi_dev->convert_fb->height = dev_cfg->height;
     disp_spi_dev->convert_fb->len = frame_len;
 
     // Configure SPI base settings
     disp_spi_dev->cfg.width = dev_cfg->width;
     disp_spi_dev->cfg.height = dev_cfg->height;
     disp_spi_dev->cfg.pixel_fmt = TUYA_PIXEL_FMT_I2;
     disp_spi_dev->cfg.port = dev_cfg->port;
     disp_spi_dev->cfg.spi_clk = dev_cfg->spi_clk;
     disp_spi_dev->cfg.cs_pin = dev_cfg->cs_pin;
     disp_spi_dev->cfg.dc_pin = dev_cfg->dc_pin;
     disp_spi_dev->cfg.rst_pin = dev_cfg->rst_pin;
 
     // Configure device info
     disp_spi_dev_info.type = TUYA_DISPLAY_SPI;
     disp_spi_dev_info.width = dev_cfg->width;
     disp_spi_dev_info.height = dev_cfg->height;
     disp_spi_dev_info.fmt = TUYA_PIXEL_FMT_I2;
     disp_spi_dev_info.rotation = dev_cfg->rotation;
     disp_spi_dev_info.is_swap = false;
 
     memcpy(&disp_spi_dev_info.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
     memcpy(&disp_spi_dev_info.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
 
     // Register display interface functions
     TDD_DISP_INTFS_T disp_spi_intfs = {
         .open = __tdd_disp_spi_epd7in5_open,
         .flush = __tdd_disp_spi_epd7in5_flush,
         .close = __tdd_disp_spi_epd7in5_close,
     };
 
     TUYA_CALL_ERR_RETURN(tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_spi_dev,
                                                  &disp_spi_intfs, &disp_spi_dev_info));
 
     PR_NOTICE("tdd_disp_spi_epd7in5_register: %s (%dx%d)", name, dev_cfg->width, dev_cfg->height);
 
     return OPRT_OK;
 }
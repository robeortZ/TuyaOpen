#ifndef __TDD_DISP_EPD7IN5_H__
#define __TDD_DISP_EPD7IN5_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"
#include "tdd_display_spi.h"

OPERATE_RET tdd_disp_spi_epd7in5_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg);

#endif
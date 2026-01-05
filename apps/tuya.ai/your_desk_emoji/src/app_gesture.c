#include "tkl_i2c.h"
#include "tkl_gpio.h"
#include "tkl_pinmux.h"
#include "tal_log.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_semaphore.h"

#include "app_gesture.h"




#define GESTURE_INT_PIN                 TUYA_GPIO_NUM_44 
#define IIC0_SDA_PIN                 	TUYA_GPIO_NUM_31
#define IIC0_SCL_PIN                 	TUYA_GPIO_NUM_30

/**
 * @file paj7620_driver.c
 * @brief PAJ7620手势传感器驱动程序实现
 * @version 2.0
 * @date 2025-01-XX
 * 
 */

#include "tal_system.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tal_mutex.h"
#include "tal_log.h"
#include "tkl_output.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
STATIC GESTURE_CB_T s_gesture_cb = NULL;

STATIC THREAD_HANDLE s_gesture_thread_handle = NULL;

/* 全局互斥锁 */
static MUTEX_HANDLE g_paj7620_mutex = NULL;

/* I2C配置 */
static TUYA_IIC_BASE_CFG_T g_paj7620_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER,
    .speed = TUYA_IIC_BUS_SPEED_400K,  // PAJ7620支持400KHz
    .addr_width = TUYA_IIC_ADDRESS_7BIT
};

/* PAJ7620初始化序列 (来自 Seeed Studio 库) */
/* PAJ7620U2_20140305.asc - Near_normal_mode_V5_6.15mm_121017 for 940nm */
static const uint8_t initRegisterArray[][2] = {
    {0xEF,0x00},
    {0x32,0x29},
    {0x33,0x01},
    {0x34,0x00},
    {0x35,0x01},
    {0x36,0x00},
    {0x37,0x07},
    {0x38,0x17},
    {0x39,0x06},
    {0x3A,0x12},
    {0x3F,0x00},
    {0x40,0x02},
    {0x41,0xFF},
    {0x42,0x01},
    {0x46,0x2D},
    {0x47,0x0F},
    {0x48,0x3C},
    {0x49,0x00},
    {0x4A,0x1E},
    {0x4B,0x00},
    {0x4C,0x20},
    {0x4D,0x00},
    {0x4E,0x1A},
    {0x4F,0x14},
    {0x50,0x00},
    {0x51,0x10},
    {0x52,0x00},
    {0x5C,0x02},
    {0x5D,0x00},
    {0x5E,0x10},
    {0x5F,0x3F},
    {0x60,0x27},
    {0x61,0x28},
    {0x62,0x00},
    {0x63,0x03},
    {0x64,0xF7},
    {0x65,0x03},
    {0x66,0xD9},
    {0x67,0x03},
    {0x68,0x01},
    {0x69,0xC8},
    {0x6A,0x40},
    {0x6D,0x04},
    {0x6E,0x00},
    {0x6F,0x00},
    {0x70,0x80},
    {0x71,0x00},
    {0x72,0x00},
    {0x73,0x00},
    {0x74,0xF0},
    {0x75,0x00},
    {0x80,0x42},
    {0x81,0x44},
    {0x82,0x04},
    {0x83,0x20},
    {0x84,0x20},
    {0x85,0x00},
    {0x86,0x10},
    {0x87,0x00},
    {0x88,0x05},
    {0x89,0x18},
    {0x8A,0x10},
    {0x8B,0x01},
    {0x8C,0x37},
    {0x8D,0x00},
    {0x8E,0xF0},
    {0x8F,0x81},
    {0x90,0x06},
    {0x91,0x06},
    {0x92,0x1E},
    {0x93,0x0D},
    {0x94,0x0A},
    {0x95,0x0A},
    {0x96,0x0C},
    {0x97,0x05},
    {0x98,0x0A},
    {0x99,0x41},
    {0x9A,0x14},
    {0x9B,0x0A},
    {0x9C,0x3F},
    {0x9D,0x33},
    {0x9E,0xAE},
    {0x9F,0xF9},
    {0xA0,0x48},
    {0xA1,0x13},
    {0xA2,0x10},
    {0xA3,0x08},
    {0xA4,0x30},
    {0xA5,0x19},
    {0xA6,0x10},
    {0xA7,0x08},
    {0xA8,0x24},
    {0xA9,0x04},
    {0xAA,0x1E},
    {0xAB,0x1E},
    {0xCC,0x19},
    {0xCD,0x0B},
    {0xCE,0x13},
    {0xCF,0x64},
    {0xD0,0x21},
    {0xD1,0x0F},
    {0xD2,0x88},
    {0xE0,0x01},
    {0xE1,0x04},
    {0xE2,0x41},
    {0xE3,0xD6},
    {0xE4,0x00},
    {0xE5,0x0C},
    {0xE6,0x0A},
    {0xE7,0x00},
    {0xE8,0x00},
    {0xE9,0x00},
    {0xEE,0x07},
    {0xEF,0x01},
    {0x00,0x1E},
    {0x01,0x1E},
    {0x02,0x0F},
    {0x03,0x10},
    {0x04,0x02},
    {0x05,0x00},
    {0x06,0xB0},
    {0x07,0x04},
    {0x08,0x0D},
    {0x09,0x0E},
    {0x0A,0x9C},
    {0x0B,0x04},
    {0x0C,0x05},
    {0x0D,0x0F},
    {0x0E,0x02},
    {0x0F,0x12},
    {0x10,0x02},
    {0x11,0x02},
    {0x12,0x00},
    {0x13,0x01},
    {0x14,0x05},
    {0x15,0x07},
    {0x16,0x05},
    {0x17,0x07},
    {0x18,0x01},
    {0x19,0x04},
    {0x1A,0x05},
    {0x1B,0x0C},
    {0x1C,0x2A},
    {0x1D,0x01},
    {0x1E,0x00},
    {0x21,0x00},
    {0x22,0x00},
    {0x23,0x00},
    {0x25,0x01},
    {0x26,0x00},
    {0x27,0x39},
    {0x28,0x7F},
    {0x29,0x08},
    {0x30,0x03},
    {0x31,0x00},
    {0x32,0x1A},
    {0x33,0x1A},
    {0x34,0x07},
    {0x35,0x07},
    {0x36,0x01},
    {0x37,0xFF},
    {0x38,0x36},
    {0x39,0x07},
    {0x3A,0x00},
    {0x3E,0xFF},
    {0x3F,0x00},
    {0x40,0x77},
    {0x41,0x40},
    {0x42,0x00},
    {0x43,0x30},
    {0x44,0xA0},
    {0x45,0x5C},
    {0x46,0x00},
    {0x47,0x00},
    {0x48,0x58},
    {0x4A,0x1E},
    {0x4B,0x1E},
    {0x4C,0x00},
    {0x4D,0x00},
    {0x4E,0xA0},
    {0x4F,0x80},
    {0x50,0x00},
    {0x51,0x00},
    {0x52,0x00},
    {0x53,0x00},
    {0x54,0x00},
    {0x57,0x80},
    {0x59,0x10},
    {0x5A,0x08},
    {0x5B,0x94},
    {0x5C,0xE8},
    {0x5D,0x08},
    {0x5E,0x3D},
    {0x5F,0x99},
    {0x60,0x45},
    {0x61,0x40},
    {0x63,0x2D},
    {0x64,0x02},
    {0x65,0x96},
    {0x66,0x00},
    {0x67,0x97},
    {0x68,0x01},
    {0x69,0xCD},
    {0x6A,0x01},
    {0x6B,0xB0},
    {0x6C,0x04},
    {0x6D,0x2C},
    {0x6E,0x01},
    {0x6F,0x32},
    {0x71,0x00},
    {0x72,0x01},
    {0x73,0x35},
    {0x74,0x00},
    {0x75,0x33},
    {0x76,0x31},
    {0x77,0x01},
    {0x7C,0x84},
    {0x7D,0x03},
    {0x7E,0x01},
};

#define INIT_REG_ARRAY_SIZE (sizeof(initRegisterArray)/sizeof(initRegisterArray[0]))

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief 写入寄存器
 */
static paj7620_err_t paj7620_write_reg(paj7620_dev_t *dev, uint8_t addr, uint8_t cmd)
{
    OPERATE_RET ret;
    uint8_t buf[2];
    
    if (dev == NULL || !dev->initialized) {
        return PAJ7620_ERR_PARAM;
    }
    
    buf[0] = addr;
    buf[1] = cmd;
    
    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, buf, 2, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("PAJ7620 write reg 0x%02X failed: %d", addr, ret);
        return PAJ7620_ERR_WRITE;
    }
    
    return PAJ7620_OK;
}

/**
 * @brief 读取寄存器
 */
static paj7620_err_t paj7620_read_reg(paj7620_dev_t *dev, uint8_t addr, uint8_t *data, uint8_t qty)
{
    OPERATE_RET ret;
    
    if (dev == NULL || data == NULL || !dev->initialized || qty == 0) {
        return PAJ7620_ERR_PARAM;
    }
    
    // 先写入寄存器地址
    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &addr, 1, TRUE);
    if (ret != OPRT_OK) {
        PR_ERR("PAJ7620 write reg addr 0x%02X failed: %d", addr, ret);
        return PAJ7620_ERR_WRITE;
    }
    
    // 读取数据
    ret = tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, data, qty, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("PAJ7620 read reg 0x%02X failed: %d", addr, ret);
        return PAJ7620_ERR_READ;
    }
    
    return PAJ7620_OK;
}

/**
 * @brief 选择寄存器Bank
 */
static paj7620_err_t paj7620_select_bank(paj7620_dev_t *dev, paj7620_bank_t bank)
{
    uint8_t bank_val = (bank == PAJ7620_BANK_0) ? PAJ7620_BANK0 : PAJ7620_BANK1;
    return paj7620_write_reg(dev, PAJ7620_REGITER_BANK_SEL, bank_val);
}

/**
 * @brief 初始化PAJ7620传感器
 */
paj7620_err_t paj7620_init(paj7620_dev_t *dev, TUYA_I2C_NUM_E i2c_port, 
                           uint8_t scl_pin, uint8_t sda_pin)
{
    OPERATE_RET ret;
    uint8_t data0 = 0, data1 = 0;
    uint32_t i;
    
    if (dev == NULL) {
        return PAJ7620_ERR_PARAM;
    }
    
    PR_INFO("PAJ7620 init start, i2c_port: %d", i2c_port);
    
    /* 创建互斥锁 */
    if (g_paj7620_mutex == NULL) {
        ret = tal_mutex_create_init(&g_paj7620_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("Mutex create failed: %d", ret);
            return PAJ7620_ERR_INIT;
        }
    }
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_paj7620_mutex);
    if (ret != OPRT_OK) {
        PR_ERR("Mutex lock failed: %d", ret);
        return PAJ7620_ERR_INIT;
    }
    
    /* 配置I2C引脚（如果提供了引脚号） */
    if (scl_pin != 0 && sda_pin != 0) {
        tkl_io_pinmux_config(scl_pin, TUYA_IIC0_SCL);
        tkl_io_pinmux_config(sda_pin, TUYA_IIC0_SDA);
        PR_INFO("I2C pins configured: SCL=%d, SDA=%d", scl_pin, sda_pin);
    }
    
    /* 初始化I2C */
    ret = tkl_i2c_init(i2c_port, &g_paj7620_i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("I2C init failed: %d", ret);
        tal_mutex_unlock(g_paj7620_mutex);
        return PAJ7620_ERR_INIT;
    }
    
    /* 初始化设备结构 */
    dev->i2c_port = i2c_port;
    dev->i2c_addr = PAJ7620_I2C_ADDR;
    dev->initialized = TRUE;  // 临时设置为TRUE以便读写寄存器
    
    /* 等待传感器稳定 (700us) */
    tal_system_sleep(1);  // 至少1ms
    
    PR_INFO("INIT SENSOR...");
    
    /* 选择Bank0 */
    paj7620_select_bank(dev, PAJ7620_BANK_0);
    paj7620_select_bank(dev, PAJ7620_BANK_0);
    
    /* 读取芯片ID */
    ret = paj7620_read_reg(dev, PAJ7620_REG_PART_ID_LOW, &data0, 1);
    if (ret != PAJ7620_OK) {
        PR_ERR("Failed to read part ID low");
        dev->initialized = FALSE;
        tal_mutex_unlock(g_paj7620_mutex);
        return ret;
    }
    
    ret = paj7620_read_reg(dev, PAJ7620_REG_PART_ID_HIGH, &data1, 1);
    if (ret != PAJ7620_OK) {
        PR_ERR("Failed to read part ID high");
        dev->initialized = FALSE;
        tal_mutex_unlock(g_paj7620_mutex);
        return ret;
    }
    
    PR_INFO("Addr0 = 0x%02X, Addr1 = 0x%02X", data0, data1);
    
    /* 验证芯片ID (应该是 0x20 和 0x76) */
    if ((data0 != 0x20) || (data1 != 0x76)) {
        PR_ERR("Invalid chip ID: expected 0x20/0x76, got 0x%02X/0x%02X", data0, data1);
        dev->initialized = FALSE;
        tal_mutex_unlock(g_paj7620_mutex);
        return PAJ7620_ERR_CHIP_ID;
    }
    
    if (data0 == 0x20) {
        PR_INFO("wake-up finish.");
    }
    
    /* 写入初始化序列 */
    PR_INFO("Writing initialization sequence (%d registers)...", INIT_REG_ARRAY_SIZE);
    for (i = 0; i < INIT_REG_ARRAY_SIZE; i++) {
        ret = paj7620_write_reg(dev, initRegisterArray[i][0], initRegisterArray[i][1]);
        if (ret != PAJ7620_OK) {
            PR_ERR("Failed to write init reg 0x%02X", initRegisterArray[i][0]);
            dev->initialized = FALSE;
            tal_mutex_unlock(g_paj7620_mutex);
            return ret;
        }
        tal_system_sleep(1);  // 每个寄存器写入后稍作延时
    }
    
    /* 选择Bank0 (手势标志寄存器在Bank0) */
    paj7620_select_bank(dev, PAJ7620_BANK_0);
    
    tal_mutex_unlock(g_paj7620_mutex);
    PR_INFO("PAJ7620 initialize register finished.");
    
    return PAJ7620_OK;
}

/**
 * @brief 反初始化PAJ7620传感器
 */
paj7620_err_t paj7620_deinit(paj7620_dev_t *dev)
{
    OPERATE_RET ret;
    
    if (dev == NULL) {
        return PAJ7620_ERR_PARAM;
    }
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_paj7620_mutex);
    if (ret != OPRT_OK) {
        return PAJ7620_ERR_PARAM;
    }
    
    /* 清除初始化标志 */
    dev->initialized = FALSE;
    
    /* 反初始化I2C */
    tkl_i2c_deinit(dev->i2c_port);
    
    tal_mutex_unlock(g_paj7620_mutex);
    
    /* 释放互斥锁 */
    if (g_paj7620_mutex != NULL) {
        tal_mutex_release(g_paj7620_mutex);
        g_paj7620_mutex = NULL;
    }
    
    PR_INFO("PAJ7620 deinit completed");
    
    return PAJ7620_OK;
}

/**
 * @brief 读取手势数据
 */
paj7620_err_t paj7620_read_gesture(paj7620_dev_t *dev, GESTURE_TYPE_E *gesture)
{
    OPERATE_RET ret;
    uint8_t gesture_data[2] = {0, 0};
    
    if (dev == NULL || gesture == NULL || !dev->initialized) {
        return PAJ7620_ERR_PARAM;
    }
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_paj7620_mutex);
    if (ret != OPRT_OK) {
        return PAJ7620_ERR_PARAM;
    }
    
    /* 确保在Bank0 */
    paj7620_select_bank(dev, PAJ7620_BANK_0);
    
    /* 读取手势标志寄存器 (0x43 和 0x44) */
    ret = paj7620_read_reg(dev, PAJ7620_ADDR_GES_PS_DET_FLAG_0, gesture_data, 2);
    if (ret != PAJ7620_OK) {
        tal_mutex_unlock(g_paj7620_mutex);
        return ret;
    }
    
    tal_mutex_unlock(g_paj7620_mutex);
    
    /* 解析手势类型 (寄存器0x43) */
    if (gesture_data[0] & GES_RIGHT_FLAG) {
        *gesture = GESTURE_RIGHT;
    } else if (gesture_data[0] & GES_LEFT_FLAG) {
        *gesture = GESTURE_LEFT;
    } else if (gesture_data[0] & GES_UP_FLAG) {
        *gesture = GESTURE_UP;
    } else if (gesture_data[0] & GES_DOWN_FLAG) {
        *gesture = GESTURE_DOWN;
    } else if (gesture_data[0] & GES_FORWARD_FLAG) {
        *gesture = GESTURE_FORWARD;
    } else if (gesture_data[0] & GES_BACKWARD_FLAG) {
        *gesture = GESTURE_BACKWARD;
    } else if (gesture_data[0] & GES_CLOCKWISE_FLAG) {
        *gesture = GESTURE_CLOCKWISE;
    } else if (gesture_data[0] & GES_COUNT_CLOCKWISE_FLAG) {
        *gesture = GESTURE_ANTICLOCKWISE;
    } else if (gesture_data[1] & GES_WAVE_FLAG) {
        /* 挥手手势在寄存器0x44 */
        *gesture = GESTURE_WAVE;
    } else {
        *gesture = GESTURE_NONE;
    }
    
    return PAJ7620_OK;
}

/**
 * @brief 检查芯片ID
 */
paj7620_err_t paj7620_check_chip_id(paj7620_dev_t *dev)
{
    OPERATE_RET ret;
    uint8_t part_id_low, part_id_high;
    
    if (dev == NULL || !dev->initialized) {
        return PAJ7620_ERR_PARAM;
    }
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_paj7620_mutex);
    if (ret != OPRT_OK) {
        return PAJ7620_ERR_PARAM;
    }
    
    /* 确保在Bank0 */
    paj7620_select_bank(dev, PAJ7620_BANK_0);
    
    /* 读取芯片ID */
    ret = paj7620_read_reg(dev, PAJ7620_REG_PART_ID_LOW, &part_id_low, 1);
    if (ret != PAJ7620_OK) {
        tal_mutex_unlock(g_paj7620_mutex);
        return ret;
    }
    
    ret = paj7620_read_reg(dev, PAJ7620_REG_PART_ID_HIGH, &part_id_high, 1);
    if (ret != PAJ7620_OK) {
        tal_mutex_unlock(g_paj7620_mutex);
        return ret;
    }
    
    tal_mutex_unlock(g_paj7620_mutex);
    
    PR_INFO("PAJ7620 Chip ID: 0x%02X%02X", part_id_high, part_id_low);
    
    /* PAJ7620的芯片ID应该是0x7620 */
    if (part_id_high == 0x76 && part_id_low == 0x20) {
        return PAJ7620_OK;
    }
    
    PR_WARN("Unexpected chip ID: 0x%02X%02X", part_id_high, part_id_low);
    return PAJ7620_ERR_CHIP_ID;
}

/**
 * @brief 获取手势名称字符串
 */
const char* paj7620_gesture_to_string(GESTURE_TYPE_E gesture)
{
    switch (gesture) {
        case GESTURE_NONE:
            return "None";
        case GESTURE_RIGHT:
            return "Right";
        case GESTURE_LEFT:
            return "Left";
        case GESTURE_UP:
            return "Up";
        case GESTURE_DOWN:
            return "Down";
        case GESTURE_FORWARD:
            return "Forward";
        case GESTURE_BACKWARD:
            return "Backward";
        case GESTURE_CLOCKWISE:
            return "Clockwise";
        case GESTURE_ANTICLOCKWISE:
            return "Anti-Clockwise";
        case GESTURE_WAVE:
            return "Wave";
        default:
            return "Unknown";
    }
}

STATIC VOID __gesture_thread_process(VOID *arg)
{
    GESTURE_TYPE_E gesture = GESTURE_NONE;
    paj7620_err_t ret;
    paj7620_dev_t *paj7620_dev = (paj7620_dev_t *)arg;

    PR_DEBUG("Gesture monitor thread started");

    while (1) {
        ret = paj7620_read_gesture(paj7620_dev, &gesture);
        if (ret == PAJ7620_OK) {
            if (gesture != GESTURE_NONE) {
                PR_INFO("Detected gesture: %s", paj7620_gesture_to_string(gesture));
                
                // Call callback if gesture detected and callback is set
                if (s_gesture_cb ) {
                    s_gesture_cb(gesture);
                }
            }

            
        } else {
            PR_ERR("Failed to read gesture: %d", ret);
        }
        
        // Small delay to prevent excessive CPU usage
        tal_system_sleep(200);
    }
}

// Modified: Initialize interface with callback parameter type
OPERATE_RET app_gesture_init(GESTURE_CB_T cb)
{
    OPERATE_RET rt = OPRT_OK;

    static paj7620_dev_t paj7620_dev = {0};
    paj7620_err_t ret;
    
    PR_INFO("PAJ7620 Example Start");
    
    /* 步骤1: 初始化PAJ7620传感器 */
    /* 参数说明：
     * - i2c_port: I2C端口号，例如 TUYA_I2C_NUM_0
     * - scl_pin: SCL引脚号，例如 TUYA_GPIO_NUM_20（传0则跳过pinmux配置）
     * - sda_pin: SDA引脚号，例如 TUYA_GPIO_NUM_21（传0则跳过pinmux配置）
     */
    ret = paj7620_init(&paj7620_dev, TUYA_I2C_NUM_0, 
                       IIC0_SCL_PIN, IIC0_SDA_PIN);
    if (ret != PAJ7620_OK) {
        PR_ERR("PAJ7620 init failed: %d", ret);
        return ret;
    }
    s_gesture_cb = cb;
    /* 步骤2: 创建手势处理线程 */
    THREAD_CFG_T thrd_param = {0};
    thrd_param.thrdname = "gesture_monitor";
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.stackDepth = 4096;  // Increase stack size to prevent overflow

    rt = tal_thread_create_and_start(&s_gesture_thread_handle, NULL, NULL, __gesture_thread_process, &paj7620_dev, &thrd_param);
    if (rt != OPRT_OK) {
        PR_ERR("tal_thread_create_and_start failed, rt: %d", rt);
    }

    PR_INFO("Gesture sensor initialized successfully");

    return rt;
}

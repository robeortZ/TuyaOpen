/**
 * @file aht20_example.c
 * @brief AHT20温湿度传感器使用示例
 * @version 1.0
 * @date 2024-08-04
 */

#include "aht20_driver.h"
#include "tal_system.h"
#include "tkl_i2c.h"
#include "tal_mutex.h"
#include "tal_thread.h"
#include "tal_log.h"
#include "tkl_output.h"
#include "tkl_pinmux.h"
#include "app_display.h"

#define IIC_SCL_PIN TUYA_IO_PIN_46
#define IIC_SDA_PIN TUYA_IO_PIN_47
/* 线程句柄 */
static THREAD_HANDLE g_aht20_thread = NULL;

/**
 * @brief AHT20传感器测试线程
 * @param arg 线程参数
 */
static void aht20_test_thread(void *arg)
{
    aht20_err_t ret;
    aht20_data_t sensor_data;
    uint32_t count = 0;
    
    PR_INFO("AHT20 test thread started");
    
    /* 配置I2C引脚映射 */
    PR_INFO("Configuring I2C pinmux...");
    ret = tkl_io_pinmux_config(IIC_SCL_PIN, TUYA_IIC1_SCL);
    if (ret != OPRT_OK) {
        PR_ERR("I2C SCL pinmux config failed: %d", ret);
        return;
    }
    PR_INFO("I2C SCL pinmux config success");
    
    ret = tkl_io_pinmux_config(IIC_SDA_PIN, TUYA_IIC1_SDA);
    if (ret != OPRT_OK) {
        PR_ERR("I2C SDA pinmux config failed: %d", ret);
        return;
    }
    PR_INFO("I2C SDA pinmux config success");
    
    /* 初始化I2C */
    PR_INFO("Initializing I2C...");
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT
    };
    
    ret = tkl_i2c_init(TUYA_I2C_NUM_1, &i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("I2C init failed: %d", ret);
        return;
    }
    PR_INFO("I2C init success");
    
    /* 初始化AHT20传感器 */
    PR_INFO("Initializing AHT20 sensor...");
    ret = aht20_init(TUYA_I2C_NUM_1);
    if (ret != AHT20_OK) {
        PR_ERR("AHT20 init failed: %d", ret);
        tkl_i2c_deinit(TUYA_I2C_NUM_1);
        return;
    }
    
    PR_INFO("AHT20 sensor initialized successfully");
    
    /* 主循环 - 每5秒读取一次数据 */
    while (1) {
        ret = aht20_read_data(TUYA_I2C_NUM_1, &sensor_data);
        if (ret == AHT20_OK) {
            PR_INFO("[%d] Temperature: %.2f°C, Humidity: %.2f%%, Status: 0x%02X", 
                count++, 
                sensor_data.temperature, 
                sensor_data.humidity, 
                sensor_data.status);
            
            /* 发送温湿度数据到显示系统 */
            // float temp_humi_data[2] = {sensor_data.temperature, sensor_data.humidity};
            // app_display_send_msg(TY_DISPLAY_TP_TEMP_HUMI, (uint8_t *)temp_humi_data, sizeof(temp_humi_data));
        } else {
            PR_ERR("AHT20 read data failed: %d", ret);
            
            /* 如果读取失败，尝试复位传感器 */
            if (ret == AHT20_ERR_BUSY || ret == AHT20_ERR_NOT_CAL) {
                PR_INFO("Trying to reset AHT20 sensor...");
                aht20_reset(TUYA_I2C_NUM_1);
                tal_system_sleep(1000);
                
                ret = aht20_init(TUYA_I2C_NUM_1);
                if (ret != AHT20_OK) {
                    PR_ERR("AHT20 re-init failed: %d", ret);
                }
            }
        }
        
        /* 等待5秒 */
        tal_system_sleep(5000);
    }
}

/**
 * @brief 启动AHT20测试
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET aht20_test_start(void)
{
    OPERATE_RET ret;
    
    /* 创建AHT20测试线程 */
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_2,
        .thrdname = "aht20_test"
    };
    
    ret = tal_thread_create_and_start(&g_aht20_thread, 
                                     NULL, 
                                     NULL, 
                                     aht20_test_thread, 
                                     NULL, 
                                     &thread_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Create AHT20 test thread failed: %d", ret);
        return ret;
    }
    
    PR_INFO("AHT20 test thread started");
    return OPRT_OK;
}

/**
 * @brief 停止AHT20测试
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET aht20_test_stop(void)
{
    if (g_aht20_thread != NULL) {
        tal_thread_delete(g_aht20_thread);
        g_aht20_thread = NULL;
        PR_INFO("AHT20 test thread stopped");
    }
    
    /* 反初始化AHT20传感器 */
    aht20_deinit(TUYA_I2C_NUM_1);
    
    /* 反初始化I2C */
    tkl_i2c_deinit(TUYA_I2C_NUM_1);
    
    return OPRT_OK;
}

/**
 * @brief 单次读取AHT20数据
 * @param temperature 温度指针
 * @param humidity 湿度指针
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET aht20_read_once(float *temperature, float *humidity)
{
    aht20_err_t ret;
    aht20_data_t sensor_data;
    
    if (temperature == NULL || humidity == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    /* 配置I2C引脚映射 */
    ret = tkl_io_pinmux_config(TUYA_IO_PIN_6, TUYA_IIC1_SCL);
    if (ret != OPRT_OK) {
        PR_ERR("I2C SCL pinmux config failed: %d", ret);
        return ret;
    }
    
    ret = tkl_io_pinmux_config(TUYA_IO_PIN_7, TUYA_IIC1_SDA);
    if (ret != OPRT_OK) {
        PR_ERR("I2C SDA pinmux config failed: %d", ret);
        return ret;
    }
    
    /* 初始化I2C */
    TUYA_IIC_BASE_CFG_T i2c_cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT
    };
    
    ret = tkl_i2c_init(TUYA_I2C_NUM_1, &i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("I2C init failed: %d", ret);
        return ret;
    }
    
    /* 初始化AHT20传感器 */
    ret = aht20_init(TUYA_I2C_NUM_1);
    if (ret != AHT20_OK) {
        PR_ERR("AHT20 init failed: %d", ret);
        tkl_i2c_deinit(TUYA_I2C_NUM_1);
        return ret;
    }
    
    ret = aht20_read_data(TUYA_I2C_NUM_1, &sensor_data);
    if (ret == AHT20_OK) {
        *temperature = sensor_data.temperature;
        *humidity = sensor_data.humidity;
        
        /* 反初始化 */
        aht20_deinit(TUYA_I2C_NUM_1);
        tkl_i2c_deinit(TUYA_I2C_NUM_1);
        
        return OPRT_OK;
    }
    
    /* 反初始化 */
    aht20_deinit(TUYA_I2C_NUM_1);
    tkl_i2c_deinit(TUYA_I2C_NUM_1);
    
    return OPRT_INVALID_PARM;
} 
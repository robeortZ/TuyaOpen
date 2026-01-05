/**
 * @file aht20_driver.c
 * @brief AHT20温湿度传感器驱动程序实现
 * @version 1.0
 * @date 2024-08-04
 */

#include "aht20_driver.h"
#include "tal_system.h"
#include "tkl_i2c.h"
#include "tal_mutex.h"
#include "tal_log.h"
#include "tkl_output.h"

/* 全局变量 */
static TUYA_I2C_NUM_E g_aht20_i2c_port = TUYA_I2C_NUM_1;
static MUTEX_HANDLE g_aht20_mutex = NULL;

/**
 * @brief 写入命令到AHT20
 * @param cmd 命令数组
 * @param len 命令长度
 * @return aht20_err_t 错误码
 */
static aht20_err_t aht20_write_cmd(uint8_t *cmd, uint8_t len)
{
    OPERATE_RET ret;
    
    if (cmd == NULL || len == 0) {
        return AHT20_ERR_PARAM;
    }
    
    PR_DEBUG("I2C write: port=%d, addr=0x%02X, len=%d", g_aht20_i2c_port, AHT20_I2C_ADDR, len);
    ret = tkl_i2c_master_send(g_aht20_i2c_port, AHT20_I2C_ADDR, cmd, len, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("I2C write failed: %d", ret);
        return AHT20_ERR_WRITE;
    }
    
    return AHT20_OK;
}

/**
 * @brief 从AHT20读取原始数据
 * @param data 数据缓冲区
 * @param len 读取长度
 * @return aht20_err_t 错误码
 */
static aht20_err_t aht20_read_raw_data(uint8_t *data, uint8_t len)
{
    OPERATE_RET ret;
    
    if (data == NULL || len == 0) {
        return AHT20_ERR_PARAM;
    }
    
    PR_DEBUG("I2C read: port=%d, addr=0x%02X, len=%d", g_aht20_i2c_port, AHT20_I2C_ADDR, len);
    ret = tkl_i2c_master_receive(g_aht20_i2c_port, AHT20_I2C_ADDR, data, len, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("I2C read failed: %d", ret);
        return AHT20_ERR_READ;
    }
    
    return AHT20_OK;
}



aht20_err_t aht20_init(TUYA_I2C_NUM_E i2c_port)
{
    OPERATE_RET ret;
    uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
    uint8_t status;
    int retry_count = 0;
    const int max_retries = 3;
    
    PR_INFO("AHT20 init start, i2c_port: %d", i2c_port);
    
    g_aht20_i2c_port = i2c_port;
    
    /* 创建互斥锁 */
    if (g_aht20_mutex == NULL) {
        ret = tal_mutex_create_init(&g_aht20_mutex);
        if (ret != OPRT_OK) {
            PR_ERR("Mutex create failed: %d", ret);
            return AHT20_ERR_INIT;
        }
        PR_INFO("Mutex created successfully");
    }
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_aht20_mutex);
    if (ret != OPRT_OK) {
        PR_ERR("Mutex lock failed: %d", ret);
        return AHT20_ERR_INIT;
    }
    
    /* 上电后等待至少40ms */
    PR_INFO("Power-on delay 40ms...");
    tal_system_sleep(50);
    
    /* 检查I2C设备是否存在 */
    PR_INFO("Checking I2C device at address 0x%02X...", AHT20_I2C_ADDR);
    uint8_t test_data = 0;
    ret = aht20_read_raw_data(&test_data, 1);
    if (ret != AHT20_OK) {
        PR_ERR("I2C device not found at address 0x%02X, error: %d", AHT20_I2C_ADDR, ret);
        tal_mutex_unlock(g_aht20_mutex);
        return AHT20_ERR_INIT;
    }
    PR_INFO("I2C device found at address 0x%02X, initial status: 0x%02X", AHT20_I2C_ADDR, test_data);
    
    /* 首先尝试复位传感器 */
    PR_INFO("Resetting AHT20 sensor first...");
    aht20_reset(i2c_port);
    tal_system_sleep(AHT20_RESET_DELAY);
    
    /* 多次尝试初始化，直到校准成功 */
    while (retry_count < max_retries) {
        PR_INFO("AHT20 init attempt %d/%d", retry_count + 1, max_retries);
        
        /* 发送初始化命令 */
        PR_INFO("Sending AHT20 init command...");
        ret = aht20_write_cmd(init_cmd, 3);
        if (ret != AHT20_OK) {
            PR_ERR("AHT20 init command failed: %d", ret);
            tal_mutex_unlock(g_aht20_mutex);
            return ret;
        }
        PR_INFO("AHT20 init command sent successfully");
        
        /* 等待初始化完成，增加等待时间 */
        PR_INFO("Waiting for initialization...");
        tal_system_sleep(100);  // 等待100ms，参考成功实现
        
        /* 检查状态 */
        PR_INFO("Reading AHT20 status...");
        ret = aht20_read_raw_data(&status, 1);
        if (ret != AHT20_OK) {
            PR_ERR("AHT20 status read failed: %d", ret);
            tal_mutex_unlock(g_aht20_mutex);
            return ret;
        }
        PR_INFO("AHT20 status: 0x%02X (binary: %d%d%d%d%d%d%d%d)", status,
                (status >> 7) & 1, (status >> 6) & 1, (status >> 5) & 1, (status >> 4) & 1,
                (status >> 3) & 1, (status >> 2) & 1, (status >> 1) & 1, status & 1);
        
        /* 检查是否已校准 - 状态第4位(bit3)为1表示已校准 */
        if ((status & 0x08) == 0x08) {
            PR_INFO("AHT20 calibration successful!");
            tal_mutex_unlock(g_aht20_mutex);
            PR_INFO("AHT20 init completed successfully");
            return AHT20_OK;
        }
        
        PR_WARN("AHT20 not calibrated, status: 0x%02X, bit2=%d, retrying...", status, (status >> 2) & 1);
        retry_count++;
        
        if (retry_count < max_retries) {
            /* 在重试前等待更长时间 */
            tal_system_sleep(2000);  // 增加重试间隔到2秒
        }
    }
    
    tal_mutex_unlock(g_aht20_mutex);
    PR_ERR("AHT20 calibration failed after %d attempts", max_retries);
    return AHT20_ERR_NOT_CAL;
}

aht20_err_t aht20_deinit(TUYA_I2C_NUM_E i2c_port)
{
    OPERATE_RET ret;
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_aht20_mutex);
    if (ret != OPRT_OK) {
        return AHT20_ERR_PARAM;
    }
    
    /* 发送睡眠命令 */
    uint8_t sleep_cmd = AHT20_CMD_SLEEP;
    aht20_write_cmd(&sleep_cmd, 1);
    
    tal_mutex_unlock(g_aht20_mutex);
    
    /* 释放互斥锁 */
    if (g_aht20_mutex != NULL) {
        tal_mutex_release(g_aht20_mutex);
        g_aht20_mutex = NULL;
    }
    
    return AHT20_OK;
}

aht20_err_t aht20_read_data(TUYA_I2C_NUM_E i2c_port, aht20_data_t *data)
{
    OPERATE_RET ret;
    uint8_t measure_cmd[3] = {AHT20_CMD_MEASURE, 0x33, 0x00};
    uint8_t raw_data[7];
    uint32_t humidity_raw, temperature_raw;
    
    if (data == NULL) {
        return AHT20_ERR_PARAM;
    }
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_aht20_mutex);
    if (ret != OPRT_OK) {
        return AHT20_ERR_BUSY;
    }
    
    /* 发送测量命令 */
    ret = aht20_write_cmd(measure_cmd, 3);
    if (ret != AHT20_OK) {
        tal_mutex_unlock(g_aht20_mutex);
        return ret;
    }
    
    /* 等待测量完成 */
    tal_system_sleep(AHT20_MEASURE_DELAY);
    
    /* 读取测量数据 */
    ret = aht20_read_raw_data(raw_data, 7);
    if (ret != AHT20_OK) {
        tal_mutex_unlock(g_aht20_mutex);
        return ret;
    }
    
    /* 检查状态 */
    if (raw_data[0] & AHT20_STATUS_BUSY) {
        tal_mutex_unlock(g_aht20_mutex);
        return AHT20_ERR_BUSY;
    }
    
    /* 解析湿度数据 */
    humidity_raw = ((uint32_t)raw_data[1] << 12) | 
                   ((uint32_t)raw_data[2] << 4) | 
                   ((uint32_t)raw_data[3] >> 4);
    data->humidity = (float)humidity_raw / 1048576.0f * 100.0f;
    
    /* 解析温度数据 */
    temperature_raw = ((uint32_t)(raw_data[3] & 0x0F) << 16) | 
                      ((uint32_t)raw_data[4] << 8) | 
                      raw_data[5];
    data->temperature = (float)temperature_raw / 1048576.0f * 200.0f - 50.0f;
    
    data->status = raw_data[0];
    
    tal_mutex_unlock(g_aht20_mutex);
    return AHT20_OK;
}

aht20_err_t aht20_reset(TUYA_I2C_NUM_E i2c_port)
{
    OPERATE_RET ret;
    uint8_t reset_cmd = AHT20_CMD_RESET;
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_aht20_mutex);
    if (ret != OPRT_OK) {
        return AHT20_ERR_BUSY;
    }
    
    /* 发送复位命令 */
    ret = aht20_write_cmd(&reset_cmd, 1);
    if (ret != AHT20_OK) {
        tal_mutex_unlock(g_aht20_mutex);
        return ret;
    }
    
    /* 等待复位完成 */
    tal_system_sleep(AHT20_RESET_DELAY);
    
    tal_mutex_unlock(g_aht20_mutex);
    return AHT20_OK;
}

aht20_err_t aht20_check_status(TUYA_I2C_NUM_E i2c_port)
{
    OPERATE_RET ret;
    uint8_t status;
    aht20_err_t aht20_ret;
    
    /* 获取互斥锁 */
    ret = tal_mutex_lock(g_aht20_mutex);
    if (ret != OPRT_OK) {
        return AHT20_ERR_BUSY;
    }
    
    /* 读取状态 */
    aht20_ret = aht20_read_raw_data(&status, 1);
    if (aht20_ret != AHT20_OK) {
        tal_mutex_unlock(g_aht20_mutex);
        return aht20_ret;
    }
    
    tal_mutex_unlock(g_aht20_mutex);
    
    /* 检查校准状态 */
    if ((status & AHT20_STATUS_CAL) != AHT20_STATUS_CAL) {
        return AHT20_ERR_NOT_CAL;
    }
    
    return AHT20_OK;
} 
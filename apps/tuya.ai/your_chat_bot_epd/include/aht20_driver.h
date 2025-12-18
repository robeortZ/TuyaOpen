/**
 * @file aht20_driver.h
 * @brief AHT20温湿度传感器驱动程序头文件
 * @version 1.0
 * @date 2024-08-04
 */

#ifndef __AHT20_DRIVER_H__
#define __AHT20_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tal_system.h"
#include "tkl_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AHT20 I2C地址 */
#define AHT20_I2C_ADDR        0x38

/* AHT20命令 */
#define AHT20_CMD_INIT        0xBE    // 初始化命令
#define AHT20_CMD_MEASURE     0xAC    // 测量命令
#define AHT20_CMD_NORMAL      0xA8    // 正常模式
#define AHT20_CMD_SLEEP       0xB0    // 睡眠模式
#define AHT20_CMD_WAKEUP      0xAB    // 唤醒命令
#define AHT20_CMD_RESET       0xBA    // 软复位命令

/* AHT20状态位 */
#define AHT20_STATUS_BUSY     0x80    // 忙状态位
#define AHT20_STATUS_CAL      0x08    // 校准状态位(bit3)

/* 延时时间定义 */
#define AHT20_INIT_DELAY      10      // 初始化延时(ms)
#define AHT20_MEASURE_DELAY   80      // 测量延时(ms)
#define AHT20_RESET_DELAY     20      // 复位延时(ms)

/* 温湿度数据结构 */
typedef struct {
    float temperature;     // 温度值(°C)
    float humidity;        // 湿度值(%)
    uint8_t status;        // 状态值
} aht20_data_t;

/* 错误码定义 */
typedef enum {
    AHT20_OK = 0,                 // 成功
    AHT20_ERR_INIT = -1,         // 初始化失败
    AHT20_ERR_PARAM = -2,        // 参数错误
    AHT20_ERR_BUSY = -3,         // 设备忙
    AHT20_ERR_TIMEOUT = -4,      // 超时
    AHT20_ERR_READ = -5,         // 读取失败
    AHT20_ERR_WRITE = -6,        // 写入失败
    AHT20_ERR_NOT_CAL = -7       // 未校准
} aht20_err_t;

/**
 * @brief 初始化AHT20传感器
 * @param i2c_port I2C端口号
 * @return aht20_err_t 错误码
 */
aht20_err_t aht20_init(TUYA_I2C_NUM_E i2c_port);

/**
 * @brief 反初始化AHT20传感器
 * @param i2c_port I2C端口号
 * @return aht20_err_t 错误码
 */
aht20_err_t aht20_deinit(TUYA_I2C_NUM_E i2c_port);

/**
 * @brief 读取AHT20温湿度数据
 * @param i2c_port I2C端口号
 * @param data 温湿度数据结构指针
 * @return aht20_err_t 错误码
 */
aht20_err_t aht20_read_data(TUYA_I2C_NUM_E i2c_port, aht20_data_t *data);

/**
 * @brief 软复位AHT20传感器
 * @param i2c_port I2C端口号
 * @return aht20_err_t 错误码
 */
aht20_err_t aht20_reset(TUYA_I2C_NUM_E i2c_port);

/**
 * @brief 检查AHT20传感器状态
 * @param i2c_port I2C端口号
 * @return aht20_err_t 错误码
 */
aht20_err_t aht20_check_status(TUYA_I2C_NUM_E i2c_port);

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_DRIVER_H__ */ 
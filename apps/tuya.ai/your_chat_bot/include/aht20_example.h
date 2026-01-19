/**
 * @file aht20_example.h
 * @brief AHT20温湿度传感器使用示例头文件
 * @version 1.0
 * @date 2024-08-04
 */

#ifndef __AHT20_EXAMPLE_H__
#define __AHT20_EXAMPLE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动AHT20测试
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET aht20_test_start(void);

/**
 * @brief 停止AHT20测试
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET aht20_test_stop(void);

/**
 * @brief 单次读取AHT20数据
 * @param temperature 温度指针
 * @param humidity 湿度指针
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET aht20_read_once(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_EXAMPLE_H__ */ 
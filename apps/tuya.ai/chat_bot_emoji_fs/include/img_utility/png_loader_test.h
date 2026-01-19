/**
* @file png_loader_test.h
* @brief PNG loader test header file
* @version 0.1
* @date 2024-01-01
*
* @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
*
*/

#ifndef __PNG_LOADER_TEST_H__
#define __PNG_LOADER_TEST_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 运行完整的PNG加载器测试套件
 * @brief Run complete PNG loader test suite
 */
void png_loader_run_tests(void);

/**
 * 简单的PNG加载测试
 * @brief Simple PNG load test
 * @param[in] filename: PNG file path to test
 * @retval OPRT_OK: Test passed
 * @retval <0: Test failed
 */
OPERATE_RET test_simple_png_load(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* __PNG_LOADER_TEST_H__ */ 
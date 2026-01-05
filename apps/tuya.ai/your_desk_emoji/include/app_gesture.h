#ifndef __APP_GESTURE_H__
#define __APP_GESTURE_H__

#include "tuya_cloud_types.h"
#include "tkl_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* PAJ7620 I2C地址 */
#define PAJ7620_I2C_ADDR                 0x73

/* PAJ7620寄存器地址 */
#define PAJ7620_REGITER_BANK_SEL        0xEF
#define PAJ7620_REG_PART_ID_LOW         0x00
#define PAJ7620_REG_PART_ID_HIGH        0x01
#define PAJ7620_ADDR_GES_PS_DET_FLAG_0  0x43
#define PAJ7620_ADDR_GES_PS_DET_FLAG_1  0x44

/* Bank选择值 */
#define PAJ7620_BANK0                    0x00
#define PAJ7620_BANK1                    0x01


/* 手势识别标志位 */
#ifndef BIT
#define BIT(x) (1UL << (x))
#endif

#define GES_UP_FLAG              BIT(0) //向上
#define GES_DOWN_FLAG            BIT(1) //向下
#define GES_LEFT_FLAG            BIT(2) //向左
#define GES_RIGHT_FLAG           BIT(3) //向右
#define GES_FORWARD_FLAG         BIT(4) //向前
#define GES_BACKWARD_FLAG        BIT(5) //向后
#define GES_CLOCKWISE_FLAG       BIT(6) //顺时针
#define GES_COUNT_CLOCKWISE_FLAG BIT(7) //逆时针
#define GES_WAVE_FLAG            BIT(8) //挥动

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* 手势类型枚举 */
typedef enum {
    GESTURE_NONE = 0,
    GESTURE_RIGHT,
    GESTURE_LEFT,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_FORWARD,
    GESTURE_BACKWARD,
    GESTURE_CLOCKWISE,
    GESTURE_ANTICLOCKWISE,
    GESTURE_WAVE
} GESTURE_TYPE_E;

/* PAJ7620手势类型（使用GESTURE_TYPE_E） */
typedef GESTURE_TYPE_E paj7620_gesture_t;

/* PAJ7620错误码 */
typedef enum {
    PAJ7620_OK = 0,
    PAJ7620_ERR_PARAM = -1,
    PAJ7620_ERR_INIT = -2,
    PAJ7620_ERR_WRITE = -3,
    PAJ7620_ERR_READ = -4,
    PAJ7620_ERR_CHIP_ID = -5
} paj7620_err_t;

/* PAJ7620 Bank类型 */
typedef enum {
    PAJ7620_BANK_0 = 0,
    PAJ7620_BANK_1 = 1
} paj7620_bank_t;

/* PAJ7620设备结构体 */
typedef struct {
    TUYA_I2C_NUM_E i2c_port;
    uint8_t i2c_addr;
    BOOL_T initialized;
} paj7620_dev_t;

/* 手势回调函数类型 */
typedef VOID (*GESTURE_CB_T)(GESTURE_TYPE_E gesture);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief 初始化手势传感器
 * @param cb 手势检测回调函数
 * @return OPERATE_RET 操作结果
 */
OPERATE_RET app_gesture_init(GESTURE_CB_T cb);

#ifdef __cplusplus
}
#endif
#endif // __APP_GESTURE_H__
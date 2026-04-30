# T5AI CORE 板 - WS2812 LED 和按键配置指南

## 📋 硬件限制

T5AI CORE 板目前定义的 GPIO：
- **GPIO 29**: 按键输入（已占用）
- **GPIO 39**: 扬声器使能脚（已占用）
- **GPIO 9**: LED 指示灯（已占用）

## 🎯 解决方案

### 方案 A：只使用 1 个按键（推荐用于 CORE 板演示）

如果 CORE 板硬件上只有 1 个按键，应用层配置如下：

```c
// 在 example_led-pixels.c 的 __buttons_init() 中
static void __buttons_init(void)
{
    button_driver_config_t btn_config;
    button_driver_get_default_config(&btn_config);
    
    /* CORE 板只用 Button 1（GPIO 29） */
    btn_config.button1_gpio.gpio_pin = TUYA_GPIO_NUM_29;
    btn_config.button1_gpio.active_level = TUYA_GPIO_LEVEL_LOW;
    btn_config.button1_callback = __btn_event_red;
    
    /* 不使用 Button 2 和 3 */
    btn_config.button2_name = NULL;
    btn_config.button3_name = NULL;
    
    OPERATE_RET rt = button_driver_init(&btn_config, &sg_button_handle);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize buttons: %d", rt);
    }
}
```

### 方案 B：使用额外的 GPIO（如果硬件支持）

如果 CORE 板的硬件还有其他可用的 GPIO 引脚，可以在应用层直接指定：

```c
/* 假设 CORE 板还有 GPIO 20、21、22 可用 */
btn_config.button1_gpio.gpio_pin = TUYA_GPIO_NUM_29;   /* 原按键 */
btn_config.button1_gpio.active_level = TUYA_GPIO_LEVEL_LOW;

btn_config.button2_gpio.gpio_pin = TUYA_GPIO_NUM_20;   /* 新增 */
btn_config.button2_gpio.active_level = TUYA_GPIO_LEVEL_LOW;

btn_config.button3_gpio.gpio_pin = TUYA_GPIO_NUM_21;   /* 新增 */
btn_config.button3_gpio.active_level = TUYA_GPIO_LEVEL_LOW;
```

## 🔧 CORE 板配置文件修改

已在 `config/TUYA_T5AI_CORE.config` 中添加：

```
CONFIG_ENABLE_LED=y
CONFIG_ENABLE_SPI=y
CONFIG_ENABLE_LEDS_PIXEL=y
```

这些配置启用了 WS2812 LED 驱动所需的 SPI 和 LED 支持。

## 📍 CORE 板的 GPIO 映射

| GPIO | 用途 | 极性 | 说明 |
|------|------|------|------|
| GPIO 29 | 按键 | LOW | 已定义 |
| GPIO 39 | 扬声器使能 | LOW | 已定义 |
| GPIO 9 | LED 指示灯 | HIGH | 已定义 |
| GPIO 2X | WS2812 SPI | - | 需硬件连接 |

## ⚠️ 注意事项

1. **WS2812 需要 SPI 接口**：确保 CORE 板硬件上已正确连接 SPI 到 WS2812 的数据引脚
2. **按键 GPIO 冲突**：如果需要 3 个按键但硬件 GPIO 不足，需要硬件工程师提供额外的 GPIO 映射
3. **查询硬件原理图**：确认 CORE 板实际可用的 GPIO 引脚

## 🚀 快速测试

使用 CORE 板单按键模式：

```c
/* 只使用 GPIO 29 按键 */
btn_config.button1_gpio.gpio_pin = TUYA_GPIO_NUM_29;
btn_config.button1_callback = __btn_event_red;  /* 所有射击都用红色 */
btn_config.button2_name = NULL;
btn_config.button3_name = NULL;

button_driver_init(&btn_config, &sg_button_handle);
```

## 📞 查询 CORE 板硬件资料

如果需要更多按键，请：
1. 查看 `/boards/T5AI/TUYA_T5AI_CORE/tuya_t5ai_core.c` 了解现有配置
2. 查看 CORE 板硬件原理图确认可用 GPIO
3. 在 board 文件中定义新的 GPIO （或在应用层直接使用）

---

**当前状态**：✅ 按键驱动已支持应用层直接指定 GPIO，无需修改 board 代码

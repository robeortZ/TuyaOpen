# Tuya T5 E-Ink Badge Template

A clean, well-structured template application for developers to start their projects on the TUYA_T5AI_EINK_NFC board.

## Features

- **Hardware Initialization**: SD card, buttons, LED, display
- **LVGL Integration**: Simple LVGL initialization and display setup
- **Button Input**: 6 buttons (UP, DOWN, LEFT, RIGHT, ENTER, RETURN) mapped as LVGL keypad input
- **Clean API Structure**: Simple, clear APIs for hardware access
- **Developer-Friendly**: Well-organized code structure for easy customization

## Project Structure

```
tuya_t5_eink_badge_template/
├── CMakeLists.txt          # Build configuration
├── Kconfig                 # Kconfig options
├── app_default.config      # Default configuration
├── config/                 # Board-specific configs
│   └── TUYA_T5AI_EINK_NFC.config
├── include/                # Public API headers
│   ├── app_hardware.h      # Hardware APIs (SD card, buttons, etc.)
│   ├── app_lvgl.h          # LVGL initialization APIs
│   └── app_input.h         # Button input APIs
├── src/                    # Source files
│   ├── main.c              # Main application entry point
│   ├── app_hardware.c      # Hardware implementation
│   ├── app_lvgl.c          # LVGL implementation
│   └── app_input.c         # Input device implementation
└── README.md               # This file
```

## Quick Start

1. **Copy this template** to your new project directory
2. **Modify `main.c`** to implement your application logic
3. **Use the provided APIs** from `include/` directory:
   - `app_hardware.h` - Hardware initialization and SD card access
   - `app_lvgl.h` - LVGL display management
   - `app_input.h` - Button input handling

## API Usage

### Hardware Initialization

```c
#include "app_hardware.h"

// Initialize all hardware
OPERATE_RET rt = app_hardware_init();

// Check SD card status
if (app_sdcard_is_mounted()) {
    const char *path = app_sdcard_get_mount_path();
    // Use SD card at path
}
```

### LVGL Initialization

```c
#include "app_lvgl.h"

// Initialize LVGL display
OPERATE_RET rt = app_lvgl_init();

// Get display handle if needed
void *display = app_lvgl_get_display();
```

### Button Input

The 6 buttons are automatically registered as LVGL keypad input:
- `btn_up` → `LV_KEY_UP`
- `btn_down` → `LV_KEY_DOWN`
- `btn_left` → `LV_KEY_LEFT`
- `btn_right` → `LV_KEY_RIGHT`
- `btn_enter` → `LV_KEY_ENTER`
- `btn_return` → `LV_KEY_ESC`

Buttons work automatically with LVGL widgets that support keypad navigation.

## Button Mapping

| Button Name | LVGL Key | Description |
|------------|----------|-------------|
| `btn_up` | `LV_KEY_UP` | Navigate up |
| `btn_down` | `LV_KEY_DOWN` | Navigate down |
| `btn_left` | `LV_KEY_LEFT` | Navigate left |
| `btn_right` | `LV_KEY_RIGHT` | Navigate right |
| `btn_enter` | `LV_KEY_ENTER` | Select/confirm |
| `btn_return` | `LV_KEY_ESC` | Cancel/back |

## Configuration

Edit `config/TUYA_T5AI_EINK_NFC.config` or `app_default.config` to customize:
- Display settings
- Button names
- LVGL features
- SD card settings

## Building

Use the standard TuyaOpen build system. The app will be built as part of the main project.

## Customization

1. **Modify `main.c`**: Add your application logic in `user_main()`
2. **Extend APIs**: Add new functions to the header/source files in `include/` and `src/`
3. **Add Features**: Create new modules following the same structure

## Notes

- All hardware is initialized in `app_hardware_init()`
- LVGL is initialized in `app_lvgl_init()`
- Buttons are automatically registered as keypad input in `app_input_init()`
- The template includes a simple demo screen to verify everything works

## License

Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.


English | [简体中文](./README_zh.md)

# your_chat_bot_epd

[your_chat_bot_epd](https://github.com/tuya/TuyaOpen/tree/master/apps/tuya.ai/your_chat_bot_epd) is an open-source large model intelligent chatbot based on tuya.ai, designed specifically for **e-Paper (EPD) display**. It features a T-shaped layout with status bar, image area, and time display, with partial refresh support for efficient updates.

## Features

1. **E-Paper Display Support**
   - 7.5 inch e-Paper display (Waveshare EPD_7IN5_V2)
   - T-shaped layout design
   - Partial refresh for time updates (every minute)
   - Full refresh for background initialization

2. **T-Layout Design**
   ```
   +--------------------------------------------------+
   |     Status Bar (Date + WiFi Status)              | 50px
   +--------------------------------------------------+
   |                      |                           |
   |     Image Area       |      Time Display         |
   |      (Left)          |        (Right)            |
   |      350x430         |        450x430            |
   |                      |                           |
   +----------------------+---------------------------+
   ```

3. **AI Image Generation & Display**
   - Parse AI responses for image URLs
   - Download JPEG images via HTTPS
   - Automatic scaling to fit display area
   - Floyd-Steinberg dithering for grayscale simulation on monochrome EPD

4. AI intelligent conversation
5. Button wake-up / Voice wake-up
6. Quick Bluetooth network connection

## Hardware Requirements

1. Audio capture (microphone)
2. Audio playback (speaker)
3. **E-Paper Display** (7.5 inch recommended)
   - Waveshare 7.5inch e-Paper V2 (800×480)
   - SPI interface connection

## Supported Hardware

| Model | Config | Description | Reset Method |
| --- | --- | --- | --- |
| TUYA T5AI_Core + 7.5" EPD | TUYA_T5AI_CORE.config | T5AI Core board with e-Paper display | Restart 3 times |
| TUYA T5AI_Board + 7.5" EPD | TUYA_T5AI_BOARD_LCD_3.5.config | T5AI Board with e-Paper display | Restart 3 times |

## Pin Configuration (Default for T5AI)

| Function | GPIO Pin |
| --- | --- |
| EPD_RST | GPIO_26 |
| EPD_DC | GPIO_24 |
| EPD_CS | GPIO_16 |
| EPD_BUSY | GPIO_25 |
| SPI_CLK | GPIO_14 |
| SPI_MOSI | GPIO_17 |

## Compilation

1. Run `tos config_choice` command to select the development board.
2. If you need to modify the configuration, run `tos menuconfig` command.
3. Run `tos build` command to compile the project.

## API Reference

### Display Functions

| Function | Description |
| --- | --- |
| `EPD_7in5_V2_init()` | Initialize e-Paper display and draw T-layout |
| `EPD_7in5_update_time(time_str)` | Partial refresh to update time display |
| `EPD_7in5_update_status(date_str, wifi)` | Partial refresh to update status bar |
| `EPD_7in5_init_layout(date, time, wifi)` | Full refresh to initialize complete layout |
| `EPD_7in5_display_downloaded_image(data, size)` | Display downloaded JPEG image with dithering |

### Refresh Strategy

- **Time Display**: Partial refresh every minute (fast, low power)
- **Status Bar**: Partial refresh when date changes
- **Background**: Full refresh only on initialization

## Notes

1. E-Paper displays have limited refresh cycles. Avoid frequent full refreshes.
2. Partial refresh is recommended for frequently updated content.
3. The display may show ghosting after many partial refreshes; occasional full refresh can clear this.

#include "epdlib/Fonts/fonts.h"
#if 1
#include "EPD_7in5_V2.h"
#include "GUI_Paint.h"
#include "tuya_cloud_types.h"
#include "tuya_log.h"
#include "tkl_system.h"
#include "tkl_spi.h"
#include "tkl_gpio.h"
#include "tkl_memory.h"
#include "Debug.h"
#include <stdlib.h>
#include <string.h>
#include "tal_api.h"

extern const unsigned char gImage_7in5_V2[];
extern const unsigned char gImage_7in5_V2_b[];
extern const unsigned char gImage_7in5_V2_ry[];

/*******************************************************************************
 * T形布局定义（重新设计）
 * 
 * +--------------------------------------------------+
 * |          状态栏（日期 + WiFi状态）               |  高度: 50px
 * +--------------------------------------------------+
 * |                      |       TODO List区域       |  高度: 200px
 * |      图片区域        +--------------------------+
 * |      (左侧)          |    时间 + 天气区域        |  高度: 230px
 * |      350x430         |       450x430             |
 * |                      |                           |
 * +----------------------+---------------------------+
 ******************************************************************************/

// 屏幕尺寸
#define SCREEN_WIDTH        EPD_7IN5_V2_WIDTH   // 800
#define SCREEN_HEIGHT       EPD_7IN5_V2_HEIGHT  // 480

// 状态栏区域（顶部）
#define STATUS_BAR_X        0
#define STATUS_BAR_Y        0
#define STATUS_BAR_WIDTH    SCREEN_WIDTH        // 800
#define STATUS_BAR_HEIGHT   50

// 分隔线
#define DIVIDER_Y           STATUS_BAR_HEIGHT
#define DIVIDER_X           350                 // 左右分隔位置

// 图片区域（左下）
#define IMAGE_AREA_X        0
#define IMAGE_AREA_Y        STATUS_BAR_HEIGHT
#define IMAGE_AREA_WIDTH    DIVIDER_X           // 350
#define IMAGE_AREA_HEIGHT   (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)  // 430

// TODO List区域（右上，原天气区域）
#define TODO_AREA_X         DIVIDER_X
#define TODO_AREA_Y         STATUS_BAR_HEIGHT
#define TODO_AREA_WIDTH     (SCREEN_WIDTH - DIVIDER_X)  // 450
#define TODO_AREA_HEIGHT    300

// 时间+天气区域（右下，合并显示）
#define TIME_AREA_X         DIVIDER_X
#define TIME_AREA_Y         (STATUS_BAR_HEIGHT + TODO_AREA_HEIGHT)  // 250
#define TIME_AREA_WIDTH     (SCREEN_WIDTH - DIVIDER_X)  // 450
#define TIME_AREA_HEIGHT    (SCREEN_HEIGHT - STATUS_BAR_HEIGHT - TODO_AREA_HEIGHT)  // 230

// 时间显示位置（在时间区域内，左侧大字体）
#define TIME_DISPLAY_X      (TIME_AREA_X + 20)
#define TIME_DISPLAY_Y      (TIME_AREA_Y + 30)

// 天气信息显示位置（在时间区域内，右侧小字体）
#define WEATHER_INFO_X      (TIME_AREA_X + 280)
#define WEATHER_INFO_Y      (TIME_AREA_Y + 20)

// 日期显示位置（在状态栏内）
#define DATE_DISPLAY_X      20
#define DATE_DISPLAY_Y      12

// WiFi状态显示位置（在状态栏内，右侧）
#define WIFI_DISPLAY_X      650
#define WIFI_DISPLAY_Y      12

// TODO List 最大条目数
#define MAX_TODO_ITEMS      6
#define TODO_ITEM_HEIGHT    28

// 缓冲区
static UBYTE *BlackImage_buf = NULL;
static UBYTE *PartialImage_buf = NULL;
static UBYTE s_epd_initialized = 0;
static UBYTE s_wifi_connected = 0;

// 当前显示的时间（用于检测变化）
static char s_current_time[16] = {0};
static char s_current_date[32] = {0};

// 天气信息结构体
typedef struct {
    int temperature;        // 温度 (摄氏度)
    int humidity;           // 湿度 (%)
    char wind_dir[16];      // 风向
    int wind_level;         // 风力等级
    char status[32];        // 天气状态 (晴/多云/阴/雨等)
    char area[64];          // 地区信息
} weather_info_t;

static weather_info_t s_weather = {
    .temperature = 0,
    .humidity = 0,
    .wind_dir = "--",
    .wind_level = 0,
    .status = "未知",
    .area = "未知"
};

// TODO List 结构体
typedef struct {
    char text[48];          // TODO 内容
    uint8_t completed;      // 是否完成 (0: 未完成, 1: 已完成)
} todo_item_t;

static todo_item_t s_todo_list[MAX_TODO_ITEMS] = {0};
static uint8_t s_todo_count = 0;


/*******************************************************************************
 * 初始化T形布局背景（全屏刷新）
 ******************************************************************************/
void EPD_7in5_init_layout(const char *date_str, const char *time_str, uint8_t wifi_connected)
{
    PR_DEBUG("EPD_7in5_init_layout start\r\n");
    
    // 计算全屏缓冲区大小
    UDOUBLE Imagesize = ((SCREEN_WIDTH % 8 == 0) ? (SCREEN_WIDTH / 8) : (SCREEN_WIDTH / 8 + 1)) * SCREEN_HEIGHT;
    if ((BlackImage_buf = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for black memory...\r\n");
        return;
    }

    // 初始化全屏模式
    EPD_7IN5_V2_Init();
    Paint_NewImage(BlackImage_buf, SCREEN_WIDTH, SCREEN_HEIGHT, 0, WHITE);
    Paint_SelectImage(BlackImage_buf);
    Paint_Clear(WHITE);

    // ========== 绘制状态栏背景 ==========
    Paint_DrawRectangle(STATUS_BAR_X, STATUS_BAR_Y, 
                        STATUS_BAR_X + STATUS_BAR_WIDTH, STATUS_BAR_Y + STATUS_BAR_HEIGHT,
                        BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    
    // 绘制日期（白色字体在黑色背景上）
    if (date_str != NULL) {
        Paint_DrawString_EN(DATE_DISPLAY_X, DATE_DISPLAY_Y, date_str, &Font24, BLACK, WHITE);
        strncpy(s_current_date, date_str, sizeof(s_current_date) - 1);
    }
    
    // 绘制WiFi状态图标
    s_wifi_connected = wifi_connected;
    // 简化WiFi显示为文字
    if (wifi_connected) {
        Paint_DrawString_EN(WIFI_DISPLAY_X, DATE_DISPLAY_Y, "WiFi:ON", &Font24, BLACK, WHITE);
    } else {
        Paint_DrawString_EN(WIFI_DISPLAY_X, DATE_DISPLAY_Y, "WiFi:xx", &Font24, BLACK, WHITE);
    }

    // ========== 绘制分隔线 ==========
    // 水平分隔线（状态栏下方）
    Paint_DrawLine(0, DIVIDER_Y, SCREEN_WIDTH, DIVIDER_Y, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    // 垂直分隔线（左右分隔）
    Paint_DrawLine(DIVIDER_X, DIVIDER_Y, DIVIDER_X, SCREEN_HEIGHT, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    // 水平分隔线（TODO区域与时间区域之间）
    Paint_DrawLine(DIVIDER_X, TIME_AREA_Y, SCREEN_WIDTH, TIME_AREA_Y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // ========== 绘制左侧图片区域 ==========
    // 绘制示例图案（可替换为实际图片）
    UWORD img_center_x = IMAGE_AREA_X + IMAGE_AREA_WIDTH / 2;
    UWORD img_center_y = IMAGE_AREA_Y + IMAGE_AREA_HEIGHT / 2;
    
    // Tuya Logo 简化图案
    Paint_DrawCircle(img_center_x, img_center_y, 80, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawCircle(img_center_x, img_center_y, 60, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(img_center_x, img_center_y, 40, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(img_center_x - 50, img_center_y + 100, "Tuya AI", &Font24, WHITE, BLACK);

    // ========== 绘制右上TODO List区域 ==========
    Paint_DrawString_EN(TODO_AREA_X + 20, TODO_AREA_Y + 10, "TODO List", &Font24, WHITE, BLACK);
    // 绘制空的TODO列表提示
    if (s_todo_count == 0) {
        Paint_DrawString_EN(TODO_AREA_X + 40, TODO_AREA_Y + 60, "No tasks yet", &Font20, WHITE, BLACK);
        Paint_DrawString_EN(TODO_AREA_X + 40, TODO_AREA_Y + 90, "Ask AI to add tasks", &Font16, WHITE, BLACK);
    } else {
        // 绘制TODO列表项
        for (int i = 0; i < s_todo_count && i < MAX_TODO_ITEMS; i++) {
            UWORD item_y = TODO_AREA_Y + 45 + i * TODO_ITEM_HEIGHT;
            // 绘制复选框
            if (s_todo_list[i].completed) {
                Paint_DrawRectangle(TODO_AREA_X + 20, item_y, TODO_AREA_X + 36, item_y + 16, 
                                    BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            } else {
                Paint_DrawRectangle(TODO_AREA_X + 20, item_y, TODO_AREA_X + 36, item_y + 16, 
                                    BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
            }
            // 绘制TODO文本
            Paint_DrawString_EN(TODO_AREA_X + 45, item_y, s_todo_list[i].text, &Font16, WHITE, BLACK);
        }
    }

    // ========== 绘制右下时间+天气区域 ==========
    // 绘制时间（大字体，左侧）
    if (time_str != NULL) {
        Paint_DrawString_EN(TIME_DISPLAY_X, TIME_DISPLAY_Y, time_str, &Font72, WHITE, BLACK);
        strncpy(s_current_time, time_str, sizeof(s_current_time) - 1);
    }
    
    // 绘制天气信息（小字体，右侧）
    // 第一行：温度 + 湿度
    char weather_line1[32];
    sprintf(weather_line1, "T:%dC H:%d%%", s_weather.temperature, s_weather.humidity);
    Paint_DrawString_EN(WEATHER_INFO_X, WEATHER_INFO_Y, weather_line1, &Font20, WHITE, BLACK);
    
    // 第二行：风力
    char weather_line2[32];
    sprintf(weather_line2, "Wind: Lv%d", s_weather.wind_level);
    Paint_DrawString_EN(WEATHER_INFO_X, WEATHER_INFO_Y + 28, weather_line2, &Font20, WHITE, BLACK);
    
    // 第三行：位置（中文）- 格式: "位置: 西湖区"
    char weather_line3[80];
    sprintf(weather_line3, "位置: %s", s_weather.area);
    Paint_DrawString_CN(WEATHER_INFO_X, WEATHER_INFO_Y + 52, weather_line3, &Font12CN_Subset, BLACK, WHITE);
    
    // 第四行：天气状态（中文）- 格式: "天气: 多云"
    char weather_line4[80];
    sprintf(weather_line4, "天气: %s", s_weather.status);
    Paint_DrawString_CN(WEATHER_INFO_X, WEATHER_INFO_Y + 72, weather_line4, &Font12CN_Subset, BLACK, WHITE);
    
    // 垂直分隔线（时间和天气之间）
    Paint_DrawLine(WEATHER_INFO_X - 15, TIME_AREA_Y + 10, 
                   WEATHER_INFO_X - 15, TIME_AREA_Y + TIME_AREA_HEIGHT - 10, 
                   BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);

    // 显示整个画面
    EPD_7IN5_V2_Display(BlackImage_buf);
    DEV_Delay_ms(500);

    // 释放缓冲区
    tkl_system_psram_free(BlackImage_buf);
    BlackImage_buf = NULL;

    s_epd_initialized = 1;
    PR_DEBUG("EPD T-layout initialized\r\n");
}

/*******************************************************************************
 * 局部刷新时间显示
 ******************************************************************************/
void EPD_7in5_update_time(const char *time_str)
{
    if (time_str == NULL) {
        PR_DEBUG("time_str is NULL\r\n");
        return;
    }

    // 检查时间是否变化
    if (strcmp(time_str, s_current_time) == 0) {
        PR_DEBUG("Time unchanged, skip refresh\r\n");
        return;
    }

    // 计算时间显示区域大小
    UWORD time_width = Font72.Width * 5;   // HH:MM = 5个字符
    UWORD time_height = Font72.Height;
    
    // 计算图像缓冲区大小
    UDOUBLE Imagesize = ((time_width % 8 == 0) ? (time_width / 8) : (time_width / 8 + 1)) * time_height;
    
    if ((PartialImage_buf = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for partial image memory...\r\n");
        return;
    }

    // 初始化局部刷新模式
    EPD_7IN5_V2_Init_Part();

    // 创建局部刷新图像
    Paint_NewImage(PartialImage_buf, time_width, time_height, 0, WHITE);
    Paint_SelectImage(PartialImage_buf);
    Paint_Clear(WHITE);

    // 绘制时间
    Paint_DrawString_EN(0, 0, time_str, &Font72, WHITE, BLACK);

    // 局部刷新
    UWORD x_end = TIME_DISPLAY_X + time_width;
    UWORD y_end = TIME_DISPLAY_Y + time_height;
    PR_DEBUG("Partial refresh time: %s at (%d,%d)-(%d,%d)\r\n", 
             time_str, TIME_DISPLAY_X, TIME_DISPLAY_Y, x_end, y_end);
    EPD_7IN5_V2_Display_Part(PartialImage_buf, TIME_DISPLAY_X, TIME_DISPLAY_Y, x_end, y_end);

    // 更新当前时间记录
    strncpy(s_current_time, time_str, sizeof(s_current_time) - 1);

    // 释放缓冲区
    tkl_system_psram_free(PartialImage_buf);
    PartialImage_buf = NULL;

    PR_DEBUG("Time updated\r\n");
}

/*******************************************************************************
 * 局部刷新状态栏（日期和WiFi状态）
 ******************************************************************************/
void EPD_7in5_update_status(const char *date_str, uint8_t wifi_connected)
{
    // 计算状态栏区域大小
    UWORD status_width = STATUS_BAR_WIDTH;
    UWORD status_height = STATUS_BAR_HEIGHT;
    
    UDOUBLE Imagesize = ((status_width % 8 == 0) ? (status_width / 8) : (status_width / 8 + 1)) * status_height;
    
    if ((PartialImage_buf = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for status bar memory...\r\n");
        return;
    }

    // 初始化局部刷新模式
    EPD_7IN5_V2_Init_Part();

    // 创建状态栏图像
    Paint_NewImage(PartialImage_buf, status_width, status_height, 0, WHITE);
    Paint_SelectImage(PartialImage_buf);
    // 状态栏背景为黑色
    Paint_Clear(BLACK);

    // 绘制日期
    if (date_str != NULL) {
        Paint_DrawString_EN(DATE_DISPLAY_X, DATE_DISPLAY_Y, date_str, &Font24, BLACK, WHITE);
        strncpy(s_current_date, date_str, sizeof(s_current_date) - 1);
    }

    // 绘制WiFi状态
    s_wifi_connected = wifi_connected;
    if (wifi_connected) {
        Paint_DrawString_EN(WIFI_DISPLAY_X, DATE_DISPLAY_Y, "WiFi:ON", &Font24, BLACK, WHITE);
    } else {
        Paint_DrawString_EN(WIFI_DISPLAY_X, DATE_DISPLAY_Y, "WiFi:--", &Font24, BLACK, WHITE);
    }

    // 局部刷新状态栏
    EPD_7IN5_V2_Display_Part(PartialImage_buf, STATUS_BAR_X, STATUS_BAR_Y, 
                              STATUS_BAR_X + status_width, STATUS_BAR_Y + status_height);

    // 释放缓冲区
    tkl_system_psram_free(PartialImage_buf);
    PartialImage_buf = NULL;

    PR_DEBUG("Status bar updated\r\n");
}

/*******************************************************************************
 * 更新左侧图片区域（可选，使用全屏刷新）
 ******************************************************************************/
void EPD_7in5_update_image(const UBYTE *image_data)
{
    if (image_data == NULL) {
        PR_DEBUG("image_data is NULL\r\n");
        return;
    }

    // 图片更新建议使用全屏刷新以获得最佳效果
    PR_DEBUG("Image update - recommend full refresh\r\n");
    // 这里可以扩展实现图片局部刷新
}

/*******************************************************************************
 * 设置天气信息（供外部调用）
 ******************************************************************************/
void EPD_7in5_set_weather(int temp, int humidity, const char *wind_dir, 
                           int wind_level, const char *status, const char *area)
{
    s_weather.temperature = temp;
    s_weather.humidity = humidity;
    if (wind_dir) {
        strncpy(s_weather.wind_dir, wind_dir, sizeof(s_weather.wind_dir) - 1);
    }
    s_weather.wind_level = wind_level;
    if (status) {
        strncpy(s_weather.status, status, sizeof(s_weather.status) - 1);
    }
    if (area) {
        strncpy(s_weather.area, area, sizeof(s_weather.area) - 1);
    }
    PR_DEBUG("Weather set: %s, %dC, %d%%, %s Lv%d, %s\r\n", 
             area, temp, humidity, wind_dir, wind_level, status);
}

/*******************************************************************************
 * 局部刷新天气信息（在时间区域右侧）
 ******************************************************************************/
void EPD_7in5_update_weather(void)
{
    // 天气信息显示在时间区域右侧，只刷新天气部分
    UWORD weather_width = TIME_AREA_WIDTH - (WEATHER_INFO_X - TIME_AREA_X);  // 约170
    UWORD weather_height = 100;  // 增加高度以容纳4行
    
    UDOUBLE Imagesize = ((weather_width % 8 == 0) ? (weather_width / 8) : (weather_width / 8 + 1)) * weather_height;
    
    if ((PartialImage_buf = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for weather area memory...\r\n");
        return;
    }

    // 初始化局部刷新模式
    EPD_7IN5_V2_Init_Part();

    // 创建天气图像（使用相对坐标）
    Paint_NewImage(PartialImage_buf, weather_width, weather_height, 0, WHITE);
    Paint_SelectImage(PartialImage_buf);
    Paint_Clear(WHITE);

    // 第一行：温度 + 湿度
    char weather_line1[32];
    sprintf(weather_line1, "T:%dC H:%d%%", s_weather.temperature, s_weather.humidity);
    Paint_DrawString_EN(0, 0, weather_line1, &Font20, WHITE, BLACK);
    
    // 第二行：风力
    char weather_line2[32];
    sprintf(weather_line2, "Wind: Lv%d", s_weather.wind_level);
    Paint_DrawString_EN(0, 28, weather_line2, &Font20, WHITE, BLACK);
    
    // 第三行：位置（中文）- 格式: "位置: 西湖区"
    char weather_line3[80];
    sprintf(weather_line3, "位置: %s", s_weather.area);
    Paint_DrawString_CN(0, 52, weather_line3, &Font12CN_Subset, BLACK, WHITE);
    
    // 第四行：天气状态（中文）- 格式: "天气: 多云"
    char weather_line4[80];
    sprintf(weather_line4, "天气: %s", s_weather.status);
    Paint_DrawString_CN(0, 72, weather_line4, &Font12CN_Subset, BLACK, WHITE);

    // 局部刷新天气区域
    EPD_7IN5_V2_Display_Part(PartialImage_buf, WEATHER_INFO_X, WEATHER_INFO_Y, 
                              WEATHER_INFO_X + weather_width, WEATHER_INFO_Y + weather_height);

    // 释放缓冲区
    tkl_system_psram_free(PartialImage_buf);
    PartialImage_buf = NULL;

    PR_DEBUG("Weather info updated\r\n");
}

/*******************************************************************************
 * 添加TODO项
 ******************************************************************************/
void EPD_7in5_add_todo(const char *text, uint8_t completed)
{
    if (s_todo_count >= MAX_TODO_ITEMS) {
        PR_DEBUG("TODO list is full\r\n");
        return;
    }
    
    strncpy(s_todo_list[s_todo_count].text, text, sizeof(s_todo_list[0].text) - 1);
    s_todo_list[s_todo_count].completed = completed;
    s_todo_count++;
    
    PR_DEBUG("TODO added: %s (count=%d)\r\n", text, s_todo_count);
}

/*******************************************************************************
 * 清空TODO列表
 ******************************************************************************/
void EPD_7in5_clear_todo(void)
{
    memset(s_todo_list, 0, sizeof(s_todo_list));
    s_todo_count = 0;
    PR_DEBUG("TODO list cleared\r\n");
}

/*******************************************************************************
 * 设置TODO项完成状态
 ******************************************************************************/
void EPD_7in5_set_todo_completed(uint8_t index, uint8_t completed)
{
    if (index < s_todo_count) {
        s_todo_list[index].completed = completed;
        PR_DEBUG("TODO %d completed=%d\r\n", index, completed);
    }
}

/*******************************************************************************
 * 删除单条TODO项
 ******************************************************************************/
void EPD_7in5_remove_todo(uint8_t index)
{
    if (index >= s_todo_count) {
        PR_DEBUG("Invalid TODO index: %d (count=%d)\r\n", index, s_todo_count);
        return;
    }
    
    PR_DEBUG("Removing TODO %d: %s\r\n", index, s_todo_list[index].text);
    
    // 将后面的项向前移动
    for (int i = index; i < s_todo_count - 1; i++) {
        memcpy(&s_todo_list[i], &s_todo_list[i + 1], sizeof(todo_item_t));
    }
    
    // 清空最后一项
    memset(&s_todo_list[s_todo_count - 1], 0, sizeof(todo_item_t));
    s_todo_count--;
    
    PR_DEBUG("TODO removed, count=%d\r\n", s_todo_count);
}

/*******************************************************************************
 * 获取TODO数量
 ******************************************************************************/
uint8_t EPD_7in5_get_todo_count(void)
{
    return s_todo_count;
}

/*******************************************************************************
 * 局部刷新TODO List区域
 ******************************************************************************/
void EPD_7in5_update_todo(void)
{
    UWORD todo_width = TODO_AREA_WIDTH;
    UWORD todo_height = TODO_AREA_HEIGHT;
    
    UDOUBLE Imagesize = ((todo_width % 8 == 0) ? (todo_width / 8) : (todo_width / 8 + 1)) * todo_height;
    
    if ((PartialImage_buf = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for TODO area memory...\r\n");
        return;
    }

    // 初始化局部刷新模式
    EPD_7IN5_V2_Init_Part();

    // 创建TODO区域图像
    Paint_NewImage(PartialImage_buf, todo_width, todo_height, 0, WHITE);
    Paint_SelectImage(PartialImage_buf);
    Paint_Clear(WHITE);

    // 标题
    Paint_DrawString_EN(20, 10, "TODO List", &Font24, WHITE, BLACK);
    
    // 绘制TODO列表
    if (s_todo_count == 0) {
        Paint_DrawString_EN(40, 60, "No tasks yet", &Font20, WHITE, BLACK);
        Paint_DrawString_EN(40, 90, "Ask AI to add tasks", &Font16, WHITE, BLACK);
    } else {
        for (int i = 0; i < s_todo_count && i < MAX_TODO_ITEMS; i++) {
            UWORD item_y = 45 + i * TODO_ITEM_HEIGHT;
            // 复选框
            if (s_todo_list[i].completed) {
                Paint_DrawRectangle(20, item_y, 36, item_y + 16, 
                                    BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            } else {
                Paint_DrawRectangle(20, item_y, 36, item_y + 16, 
                                    BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
            }
            // TODO文本
            Paint_DrawString_EN(45, item_y, s_todo_list[i].text, &Font16, WHITE, BLACK);
        }
    }

    // 局部刷新TODO区域
    EPD_7IN5_V2_Display_Part(PartialImage_buf, TODO_AREA_X, TODO_AREA_Y, 
                              TODO_AREA_X + todo_width, TODO_AREA_Y + todo_height);

    // 释放缓冲区
    tkl_system_psram_free(PartialImage_buf);
    PartialImage_buf = NULL;

    PR_DEBUG("TODO area updated\r\n");
}

/*******************************************************************************
 * 兼容旧接口：显示时间
 ******************************************************************************/
void EPD_7in5_show_time(char *time_str)
{
    if (!s_epd_initialized) {
        // 使用默认值初始化布局
        EPD_7in5_init_layout("2024-12-18", time_str, 0);
    } else {
        EPD_7in5_update_time(time_str);
    }
}

/*******************************************************************************
 * 主初始化函数 - 初始化T形布局
 ******************************************************************************/
OPERATE_RET EPD_7in5_V2_init(void)
{
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    PR_DEBUG("e-Paper Init and Clear...\r\n");
    EPD_7IN5_V2_Init();
    EPD_7IN5_V2_Clear();
    DEV_Delay_ms(500);

    // 添加示例 TODO 项
    EPD_7in5_add_todo("Fri 5pm Weekly Meeting", 0);
    EPD_7in5_add_todo("Exercise 1 hour", 0);
    EPD_7in5_add_todo("EPD add Todo feature", 1);  // 已完成
    EPD_7in5_add_todo("Play phone 1 hour", 0);

    // 初始化T形布局（日期、时间、WiFi状态）
    // 这里使用默认值，实际使用时应该传入真实的日期时间
    EPD_7in5_init_layout("2024-12-18 Wed", "15:30", 1);
    
    return OPRT_OK;
}

/*******************************************************************************
 * 每分钟刷新时间的回调函数（由定时器调用）
 ******************************************************************************/
void EPD_7in5_minute_refresh(UBYTE hour, UBYTE min)
{
    char time_str[16] = {0};
    sprintf(time_str, "%02d:%02d", hour, min);
    
    if (!s_epd_initialized) {
        PR_DEBUG("EPD not initialized, skip refresh\r\n");
        return;
    }
    
    EPD_7in5_update_time(time_str);
}

/*******************************************************************************
 * 刷新完整状态（日期、时间、WiFi、天气、TODO）
 ******************************************************************************/
void EPD_7in5_refresh_all(const char *date_str, const char *time_str, uint8_t wifi_connected)
{
    if (!s_epd_initialized) {
        // 首次初始化
        EPD_7in5_init_layout(date_str, time_str, wifi_connected);
    } else {
        // 分别更新各区域
        EPD_7in5_update_status(date_str, wifi_connected);
        EPD_7in5_update_todo();
        EPD_7in5_update_time(time_str);
        EPD_7in5_update_weather();
    }
}



OPERATE_RET EPD_7in5_V2_test(void)
{	
    PR_DEBUG("EPD_7IN5_V2_test Demo\r\n");
    if(DEV_Module_Init()!=0){
        return -1;
    }

    PR_DEBUG("e-Paper Init and Clear...\r\n");
    EPD_7IN5_V2_Init();

    EPD_7IN5_V2_Clear();
    DEV_Delay_ms(500);


    //Create a new image cache
    UBYTE *BlackImage;
    /* you have to edit the startup_stm32fxxx.s file and set a big enough heap size */
    UDOUBLE Imagesize = ((EPD_7IN5_V2_WIDTH % 8 == 0)? (EPD_7IN5_V2_WIDTH / 8 ): (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;
    if((BlackImage = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for black memory...\r\n");
        return OPRT_MALLOC_FAILED;
    }
    PR_DEBUG("Paint_NewImage\r\n");
    Paint_NewImage(BlackImage, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);     

#if 0   // show image for array   
    EPD_7IN5_V2_Init_Fast();
    PR_DEBUG("show image for array\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(gImage_7in5_V2);
    EPD_7IN5_V2_Display(BlackImage);
    DEV_Delay_ms(2000);
#endif

#if 0  // Drawing on the image
    //1.Select Image
    EPD_7IN5_V2_Init();
    PR_DEBUG("SelectImage:BlackImage\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);

    // 2.Drawing on the image
    PR_DEBUG("Drawing:BlackImage\r\n");
    Paint_DrawPoint(10, 80, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 90, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 100, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);
    Paint_DrawLine(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(70, 70, 20, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawRectangle(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(80, 70, 130, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(45, 95, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(105, 95, 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(85, 95, 125, 95, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(105, 75, 105, 115, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawString_EN(10, 0, "waveshare", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 20, "hello world", &Font12, WHITE, BLACK);
    Paint_DrawNum(10, 33, 123456789, &Font12, BLACK, WHITE);
    Paint_DrawNum(10, 50, 987654321, &Font16, WHITE, BLACK);
    Paint_DrawString_CN(130, 0, " ���abc", &Font24CN, BLACK, WHITE);
    Paint_DrawString_CN(130, 20, "΢ѩ����", &Font24CN, WHITE, BLACK);

    PR_DEBUG("EPD_Display\r\n");
    EPD_7IN5_V2_Display(BlackImage);
    DEV_Delay_ms(2000);
#endif

#if 0   //Partial refresh, example shows time
    EPD_7IN5_V2_Init_Part();
	Paint_NewImage(BlackImage, Font24.Width * 7, Font24.Height, 0, WHITE);
    Debug("Partial refresh\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
	
    PAINT_TIME sPaint_time;
    sPaint_time.Hour = 12;
    sPaint_time.Min = 34;
    sPaint_time.Sec = 56;
    UBYTE num = 5;
    for (;;) {
        sPaint_time.Sec = sPaint_time.Sec + 1;
        if (sPaint_time.Sec == 60) {
            sPaint_time.Min = sPaint_time.Min + 1;
            sPaint_time.Sec = 0;
            if (sPaint_time.Min == 60) {
                sPaint_time.Hour =  sPaint_time.Hour + 1;
                sPaint_time.Min = 0;
                if (sPaint_time.Hour == 24) {
                    sPaint_time.Hour = 0;
                    sPaint_time.Min = 0;
                    sPaint_time.Sec = 0;
                }
            }
        }
        Paint_ClearWindows(0, 0, Font24.Width * 7, Font24.Height, WHITE);
        char sPaint_time_str[10]={0};
        sprintf(sPaint_time_str, "%02d:%02d:%02d", sPaint_time.Hour, sPaint_time.Min, sPaint_time.Sec);
        Paint_DrawString_EN(0,0, sPaint_time_str,&Font24 , WHITE, BLACK);

        num = num - 1;
        if(num == 0) {
            break;
        }
		EPD_7IN5_V2_Display_Part(BlackImage, 0, 0, Font24.Width * 7, Font24.Height);
        DEV_Delay_ms(1000);//Analog clock 1s
    }
#endif

/*
    The feature will only be available on screens sold after 24/10/23
*/
#if 0 // show image for array
    free(BlackImage);
    PR_DEBUG("show Gray------------------------\r\n");
    Imagesize = ((EPD_7IN5_V2_WIDTH % 4 == 0)? (EPD_7IN5_V2_WIDTH / 4 ): (EPD_7IN5_V2_WIDTH / 4 + 1)) * EPD_7IN5_V2_HEIGHT;
    if((BlackImage = (UBYTE *)tkl_system_psram_malloc(Imagesize)) == NULL) {
        PR_DEBUG("Failed to apply for black memory...\r\n");
        while (1);
    }
    EPD_7IN5_V2_Init_4Gray();
    PR_DEBUG("4 grayscale display\r\n");
    
    Paint_NewImage(BlackImage, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
    Paint_SetScale(4);
    Paint_Clear(0xff);


    
    Paint_DrawPoint(10, 80, GRAY4, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 90, GRAY4, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 100, GRAY4, DOT_PIXEL_3X3, DOT_STYLE_DFT);
    Paint_DrawLine(20, 70, 70, 120, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(70, 70, 20, 120, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawRectangle(20, 70, 70, 120, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(80, 70, 130, 120, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(45, 95, 20, GRAY4, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(105, 95, 20, GRAY2, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(85, 95, 125, 95, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(105, 75, 105, 115, GRAY4, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawString_EN(380, 40, "Tuya AI", &Font24, GRAY4, GRAY1);
    // Paint_DrawString_EN(10, 20, "hello world!", &Font12, GRAY3, GRAY1);
    // Paint_DrawNum(10, 33, 123456789, &Font12, GRAY4, GRAY2);
    // Paint_DrawNum(10, 50, 987654321, &Font16, GRAY1, GRAY4);
    // Paint_DrawString_CN(150, 0,"���Ϳѻ����?", &Font12CN, GRAY4, GRAY1);
    // Paint_DrawString_CN(150, 20,"���abc", &Font12CN, GRAY3, GRAY2);
    // Paint_DrawString_CN(150, 40,"���abc", &Font12CN, GRAY2, GRAY3);
    // Paint_DrawString_CN(150, 60,"���abc", &Font12CN, GRAY1, GRAY4);
    // Paint_DrawString_CN(10, 130, "你好涂鸦", &Font12CN, GRAY1, GRAY4);
    // Paint_DrawString_EN(380, 130, "09:21", &Font72, GRAY1, GRAY4);
    EPD_7IN5_V2_Display_4Gray(BlackImage);
    DEV_Delay_ms(3000);

    Paint_DrawString_EN(200, 130, "09:21", &Font72, GRAY1, GRAY4);
    EPD_7IN5_V2_Display_4Gray(BlackImage);


#endif

    // PR_DEBUG("Clear...\r\n");
    // EPD_7IN5_V2_Init();
    // EPD_7IN5_V2_Clear();

    PR_DEBUG("Goto Sleep...\r\n");
    // EPD_7IN5_V2_Sleep();
    free(BlackImage);
    BlackImage = NULL;
    DEV_Delay_ms(2000);//important, at least 2s
    // close 5V
    PR_DEBUG("close 5V, Module enters 0 power consumption ...\r\n");
    // DEV_Module_Exit();
    
    return 0;
}

// JPEG 解码器 - 直接声明函数避免头文件依赖问题
typedef struct {
    int ok;
    uint16_t pixel_x;
    uint16_t pixel_y;
    uint32_t size;
} sw_jpeg_dec_res_t;

typedef enum {
    JD_FMT_RGB888 = 0,
    JD_FMT_RGB565 = 1,
    JD_FMT_Grayscale = 2,
} JD_FORMAT_OUTPUT_T;

// 媒体旋转角度（避免与 GUI_Paint.h 中的宏冲突）
#define MEDIA_ROTATE_NONE   0
#define MEDIA_ROTATE_90     1
#define MEDIA_ROTATE_180    2
#define MEDIA_ROTATE_270    3

// 声明 JPEG 解码函数
extern int bk_jpeg_get_img_info(uint32_t frame_size, uint8_t *src_buf, sw_jpeg_dec_res_t *result, uint8_t *workbuf);
extern int bk_jpeg_dec_sw_start_one_time(
    uint8_t decode_type,
    uint8_t *src_buf,
    uint8_t *dst_buf,
    uint32_t jpeg_size,
    uint32_t outbuf_size,
    sw_jpeg_dec_res_t *result,
    uint8_t scale,
    int format,
    int rotate_angle,
    uint8_t *work_buffer,
    uint8_t *rotate_buffer);

/*******************************************************************************
 * 显示下载的图片（JPEG 格式）
 * 
 * 由于墨水屏是单色的，需要将 JPEG 图片转换为单色位图
 * 使用缩放解码 + 阈值二值化方法
 * 
 * 注意：AI 生成的图片通常是 2048x2048，需要使用 1/8 缩放（scale=3）来减少内存使用
 ******************************************************************************/
void EPD_7in5_display_downloaded_image(uint8_t *jpeg_data, uint32_t jpeg_size)
{
    if (!jpeg_data || jpeg_size == 0) {
        PR_DEBUG("Invalid image data\r\n");
        return;
    }

    PR_INFO("Displaying downloaded image, size: %lu bytes\r\n", jpeg_size);

    // 目标显示区域
    uint16_t dst_w = IMAGE_AREA_WIDTH;   // 350
    uint16_t dst_h = IMAGE_AREA_HEIGHT;  // 430
    
    // 分配工作缓冲区
    uint8_t *work_buf = tkl_system_psram_malloc(12 * 1024);
    if (!work_buf) {
        PR_ERR("Failed to allocate work buffer\r\n");
        return;
    }
    
    // 1. 先获取图片信息
    sw_jpeg_dec_res_t img_info = {0};
    int ret = bk_jpeg_get_img_info(jpeg_size, jpeg_data, &img_info, work_buf);
    if (ret != 0) {
        PR_ERR("Failed to get JPEG info: %d\r\n", ret);
        tkl_system_psram_free(work_buf);
        return;
    }
    
    uint16_t orig_w = img_info.pixel_x;
    uint16_t orig_h = img_info.pixel_y;
    
    // 如果高度为 0，假设是正方形图片
    if (orig_h == 0) {
        orig_h = orig_w;
        PR_WARN("JPEG height is 0, assuming square image: %dx%d\r\n", orig_w, orig_h);
    }
    
    PR_INFO("Original JPEG: %dx%d pixels\r\n", orig_w, orig_h);
    
    // 2. 计算合适的缩放比例
    uint8_t decode_scale = 0;  // 0=1/1, 1=1/2, 2=1/4, 3=1/8
    uint16_t scaled_w = orig_w;
    uint16_t scaled_h = orig_h;
    
    // 计算缩放后尺寸不超过 600x600（约 360KB 灰度缓冲区）
    while ((scaled_w > 600 || scaled_h > 600) && decode_scale < 3) {
        decode_scale++;
        scaled_w = orig_w >> decode_scale;
        scaled_h = orig_h >> decode_scale;
    }
    
    PR_INFO("Using scale 1/%d, decoded size: %dx%d\r\n", 1 << decode_scale, scaled_w, scaled_h);
    
    // 3. 分配灰度图像缓冲区
    uint32_t gray_size = scaled_w * scaled_h;
    uint8_t *gray_buf = tkl_system_psram_malloc(gray_size);
    if (!gray_buf) {
        PR_ERR("Failed to allocate grayscale buffer (%lu bytes)\r\n", gray_size);
        tkl_system_psram_free(work_buf);
        return;
    }

    // 4. 解码 JPEG 为灰度图像（带缩放）
    sw_jpeg_dec_res_t dec_result = {0};
    ret = bk_jpeg_dec_sw_start_one_time(
        1,                      // decode_type: 1 = 按帧解码
        jpeg_data,              // 源 JPEG 数据
        gray_buf,               // 目标缓冲区
        jpeg_size,              // JPEG 大小
        gray_size,              // 输出缓冲区大小
        &dec_result,            // 解码结果
        decode_scale,           // 缩放比例
        JD_FMT_Grayscale,       // 输出格式: 灰度
        MEDIA_ROTATE_NONE,      // 不旋转
        work_buf,               // 工作缓冲区
        NULL                    // 旋转缓冲区（不需要）
    );

    tkl_system_psram_free(work_buf);

    if (ret != 0 || !dec_result.ok) {
        PR_ERR("Failed to decode JPEG: %d, ok=%d\r\n", ret, dec_result.ok);
        tkl_system_psram_free(gray_buf);
        return;
    }

    // 使用计算出的尺寸，不依赖 dec_result 的 pixel_y
    uint16_t src_w = scaled_w;
    uint16_t src_h = scaled_h;
    PR_INFO("JPEG decoded successfully: %dx%d pixels\r\n", src_w, src_h);

    // 计算显示缩放比例
    float scale_x = (float)dst_w / src_w;
    float scale_y = (float)dst_h / src_h;
    float display_scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    uint16_t final_w = (uint16_t)(src_w * display_scale);
    uint16_t final_h = (uint16_t)(src_h * display_scale);
    uint16_t offset_x = (dst_w - final_w) / 2;
    uint16_t offset_y = (dst_h - final_h) / 2;

    // 分配 EPD 图像缓冲区
    UDOUBLE epd_buf_size = ((dst_w % 8 == 0) ? (dst_w / 8) : (dst_w / 8 + 1)) * dst_h;
    UBYTE *epd_buf = (UBYTE *)tkl_system_psram_malloc(epd_buf_size);
    if (!epd_buf) {
        PR_ERR("Failed to allocate EPD buffer\r\n");
        tkl_system_psram_free(gray_buf);
        return;
    }

    // 初始化图像缓冲区
    Paint_NewImage(epd_buf, dst_w, dst_h, 0, WHITE);
    Paint_SelectImage(epd_buf);
    Paint_Clear(WHITE);

    PR_INFO("Converting with Floyd-Steinberg dithering: %dx%d -> %dx%d\r\n", 
            src_w, src_h, final_w, final_h);

    // 分配缩放后的灰度缓冲区用于抖动处理（使用 int16_t 存储误差）
    int16_t *scaled_buf = (int16_t *)tkl_system_psram_malloc(final_w * final_h * sizeof(int16_t));
    if (!scaled_buf) {
        PR_ERR("Failed to allocate scaled buffer for dithering\r\n");
        // 降级到简单阈值
        for (uint16_t y = 0; y < final_h; y++) {
            for (uint16_t x = 0; x < final_w; x++) {
                uint16_t sample_x = (uint16_t)(x / display_scale);
                uint16_t sample_y = (uint16_t)(y / display_scale);
                if (sample_x >= src_w) sample_x = src_w - 1;
                if (sample_y >= src_h) sample_y = src_h - 1;
                uint8_t gray_value = gray_buf[sample_y * src_w + sample_x];
                Paint_SetPixel(offset_x + x, offset_y + y, (gray_value < 128) ? BLACK : WHITE);
            }
        }
        tkl_system_psram_free(gray_buf);
    } else {
        // 先缩放灰度图像
        for (uint16_t y = 0; y < final_h; y++) {
            for (uint16_t x = 0; x < final_w; x++) {
                uint16_t sample_x = (uint16_t)(x / display_scale);
                uint16_t sample_y = (uint16_t)(y / display_scale);
                if (sample_x >= src_w) sample_x = src_w - 1;
                if (sample_y >= src_h) sample_y = src_h - 1;
                scaled_buf[y * final_w + x] = (int16_t)gray_buf[sample_y * src_w + sample_x];
            }
        }
        
        tkl_system_psram_free(gray_buf);  // 释放原始灰度缓冲区
        
        // Floyd-Steinberg 抖动算法
        // 误差扩散矩阵:
        //       * 7/16
        // 3/16 5/16 1/16
        for (uint16_t y = 0; y < final_h; y++) {
            for (uint16_t x = 0; x < final_w; x++) {
                int16_t old_pixel = scaled_buf[y * final_w + x];
                
                // 限制范围
                if (old_pixel < 0) old_pixel = 0;
                if (old_pixel > 255) old_pixel = 255;
                
                // 量化为黑或白
                uint8_t new_pixel = (old_pixel < 128) ? 0 : 255;
                
                // 绘制像素
                Paint_SetPixel(offset_x + x, offset_y + y, (new_pixel == 0) ? BLACK : WHITE);
                
                // 计算量化误差
                int16_t error = old_pixel - new_pixel;
                
                // 将误差扩散到相邻像素
                if (x + 1 < final_w) {
                    scaled_buf[y * final_w + (x + 1)] += error * 7 / 16;
                }
                if (y + 1 < final_h) {
                    if (x > 0) {
                        scaled_buf[(y + 1) * final_w + (x - 1)] += error * 3 / 16;
                    }
                    scaled_buf[(y + 1) * final_w + x] += error * 5 / 16;
                    if (x + 1 < final_w) {
                        scaled_buf[(y + 1) * final_w + (x + 1)] += error * 1 / 16;
                    }
                }
            }
        }
        
        tkl_system_psram_free(scaled_buf);
    }

    // 使用全屏刷新显示图片
    EPD_7IN5_V2_Init();
    
    UWORD x_end = IMAGE_AREA_X + dst_w;
    UWORD y_end = IMAGE_AREA_Y + dst_h;
    
    PR_DEBUG("Display image area: (%d,%d)-(%d,%d)\r\n", 
             IMAGE_AREA_X, IMAGE_AREA_Y, x_end, y_end);
    
    EPD_7IN5_V2_Display_Part(epd_buf, IMAGE_AREA_X, IMAGE_AREA_Y, x_end, y_end);

    tkl_system_psram_free(epd_buf);
    
    PR_INFO("Image display complete\r\n");
}

#endif
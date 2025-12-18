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
 * T形布局定义
 * 
 * +--------------------------------------------------+
 * |          状态栏（日期 + WiFi状态）               |  高度: 50px
 * +--------------------------------------------------+
 * |                      |                           |
 * |      图片区域        |       时间区域            |
 * |      (左侧)          |       (右侧)              |
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

// 时间区域（右下）
#define TIME_AREA_X         DIVIDER_X
#define TIME_AREA_Y         STATUS_BAR_HEIGHT
#define TIME_AREA_WIDTH     (SCREEN_WIDTH - DIVIDER_X)  // 450
#define TIME_AREA_HEIGHT    (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)  // 430

// 时间显示位置（在时间区域内居中）
#define TIME_DISPLAY_X      (TIME_AREA_X + 50)
#define TIME_DISPLAY_Y      (TIME_AREA_Y + 150)

// 日期显示位置（在状态栏内）
#define DATE_DISPLAY_X      20
#define DATE_DISPLAY_Y      12

// WiFi状态显示位置（在状态栏内，右侧）
#define WIFI_DISPLAY_X      650
#define WIFI_DISPLAY_Y      12

// 缓冲区
static UBYTE *BlackImage_buf = NULL;
static UBYTE *PartialImage_buf = NULL;
static UBYTE s_epd_initialized = 0;
static UBYTE s_wifi_connected = 0;

// 当前显示的时间（用于检测变化）
static char s_current_time[16] = {0};
static char s_current_date[32] = {0};


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
        Paint_DrawString_EN(WIFI_DISPLAY_X, DATE_DISPLAY_Y, "WiFi:--", &Font24, BLACK, WHITE);
    }

    // ========== 绘制分隔线 ==========
    // 水平分隔线（状态栏下方）
    Paint_DrawLine(0, DIVIDER_Y, SCREEN_WIDTH, DIVIDER_Y, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    // 垂直分隔线（左右分隔）
    Paint_DrawLine(DIVIDER_X, DIVIDER_Y, DIVIDER_X, SCREEN_HEIGHT, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

    // ========== 绘制左侧图片区域 ==========
    // 绘制示例图案（可替换为实际图片）
    UWORD img_center_x = IMAGE_AREA_X + IMAGE_AREA_WIDTH / 2;
    UWORD img_center_y = IMAGE_AREA_Y + IMAGE_AREA_HEIGHT / 2;
    
    // Tuya Logo 简化图案
    Paint_DrawCircle(img_center_x, img_center_y, 80, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawCircle(img_center_x, img_center_y, 60, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(img_center_x, img_center_y, 40, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(img_center_x - 50, img_center_y + 100, "Tuya AI", &Font24, WHITE, BLACK);

    // ========== 绘制右侧时间区域 ==========
    // 时间区域标题
    Paint_DrawString_EN(TIME_AREA_X + 20, TIME_AREA_Y + 20, "Current Time", &Font24, WHITE, BLACK);
    
    // 绘制时间（大字体）
    if (time_str != NULL) {
        Paint_DrawString_EN(TIME_DISPLAY_X, TIME_DISPLAY_Y, time_str, &Font72, WHITE, BLACK);
        strncpy(s_current_time, time_str, sizeof(s_current_time) - 1);
    }
    
    // 绘制装饰线
    Paint_DrawLine(TIME_AREA_X + 20, TIME_DISPLAY_Y - 20, 
                   TIME_AREA_X + TIME_AREA_WIDTH - 20, TIME_DISPLAY_Y - 20, 
                   BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(TIME_AREA_X + 20, TIME_DISPLAY_Y + 90, 
                   TIME_AREA_X + TIME_AREA_WIDTH - 20, TIME_DISPLAY_Y + 90, 
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
 * 刷新完整状态（日期、时间、WiFi）
 ******************************************************************************/
void EPD_7in5_refresh_all(const char *date_str, const char *time_str, uint8_t wifi_connected)
{
    if (!s_epd_initialized) {
        // 首次初始化
        EPD_7in5_init_layout(date_str, time_str, wifi_connected);
    } else {
        // 分别更新各区域
        EPD_7in5_update_status(date_str, wifi_connected);
        EPD_7in5_update_time(time_str);
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

#if 1   // show image for array   
    EPD_7IN5_V2_Init_Fast();
    PR_DEBUG("show image for array\r\n");
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(gImage_7in5_V2);
    EPD_7IN5_V2_Display(BlackImage);
    DEV_Delay_ms(2000);
#endif

#if 1  // Drawing on the image
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

#if 1   //Partial refresh, example shows time
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

#endif
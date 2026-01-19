/**
 * @file app_system_info.c
 * @brief app_system_info module is used to
 * @version 0.1
 * @date 2025-04-28
 */

#include "app_system_info.h"
#include "ai_audio_player.h"

#include "app_display.h"
#include "app_chat_bot.h"

#include "tal_api.h"
#include "tuya_iot.h"
#include "netmgr.h"
#include "tuya_weather.h"

#include "tkl_wifi.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define FREE_HEAP_TM        (10 * 1000)
#define DISPLAY_STATUS_TM   (1 * 1000)
#define WEATHER_UPDATE_TM   (30 * 60 * 1000)  // 30 minutes

// Display status message
typedef enum {
    DISPLAY_STATUS_VERSION = 0,
    DISPLAY_STATUS_STANDBY,
    DISPLAY_STATUS_TIME,
} SI_DISPLAY_STATUS_E;

TIMER_ID epd_time_update_tm;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TIMER_ID heap_tm;

    TIMER_ID display_status_tm;
    UI_WIFI_STATUS_E last_net_status;

    SI_DISPLAY_STATUS_E display_status;

    uint8_t hour;
    uint8_t min;
    
    TIMER_ID weather_tm;
} APP_SYSTEM_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
// static  void __app_display_status_time_update(TIMER_ID timer_id,  void *arg);
/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_SYSTEM_INFO_T system_info = {0};

/***********************************************************
***********************function define**********************
***********************************************************/


/**
 * @brief 将天气代码转换为中文描述
 * @param weather_code 天气代码（来自tuya_weather.h中的定义）
 * @param weather_str 输出的天气描述字符串
 * @param str_len 字符串缓冲区长度
 */
 static void weather_code_to_string(int weather_code, char *weather_str, size_t str_len)
 {
     switch(weather_code) {
         case TW_WEATHER_SUNNY:
         case TW_WEATHER_CLEAR:
             snprintf(weather_str, str_len, "晴");
             break;
         case TW_WEATHER_MOSTLY_CLEAR:
             snprintf(weather_str, str_len, "大部晴朗");
             break;
         case TW_WEATHER_PARTLY_CLOUDY:
             snprintf(weather_str, str_len, "多云");
             break;
         case TW_WEATHER_CLOUDY:
             snprintf(weather_str, str_len, "阴");
             break;
         case TW_WEATHER_OVERCAST:
             snprintf(weather_str, str_len, "阴天");
             break;
         case TW_WEATHER_FOG:
             snprintf(weather_str, str_len, "雾");
             break;
         case TW_WEATHER_FREEZING_FOG:
             snprintf(weather_str, str_len, "冻雾");
             break;
         case TW_WEATHER_HAZE:
             snprintf(weather_str, str_len, "霾");
             break;
         case TW_WEATHER_LIGHT_RAIN:
             snprintf(weather_str, str_len, "小雨");
             break;
         case TW_WEATHER_MODERATE_RAIN:
             snprintf(weather_str, str_len, "中雨");
             break;
         case TW_WEATHER_RAIN:
             snprintf(weather_str, str_len, "雨");
             break;
         case TW_WEATHER_HEAVY_RAIN:
             snprintf(weather_str, str_len, "大雨");
             break;
         case TW_WEATHER_RAINSTORM:
             snprintf(weather_str, str_len, "暴雨");
             break;
         case TW_WEATHER_EXTREME_RAINSTORM:
             snprintf(weather_str, str_len, "大暴雨");
             break;
         case TW_WEATHER_DOWNPOUR:
             snprintf(weather_str, str_len, "特大暴雨");
             break;
         case TW_WEATHER_LIGHT_TO_MODERATE_RAIN:
             snprintf(weather_str, str_len, "小到中雨");
             break;
         case TW_WEATHER_MODERATE_TO_HEAVY_RAIN:
             snprintf(weather_str, str_len, "中到大雨");
             break;
         case TW_WEATHER_HEAVY_RAIN_TO_RAINSTORM:
             snprintf(weather_str, str_len, "大到暴雨");
             break;
         case TW_WEATHER_SHOWER:
             snprintf(weather_str, str_len, "阵雨");
             break;
         case TW_WEATHER_LIGHT_SHOWER:
             snprintf(weather_str, str_len, "小阵雨");
             break;
         case TW_WEATHER_HEAVY_SHOWER:
             snprintf(weather_str, str_len, "大阵雨");
             break;
         case TW_WEATHER_ISOLATED_SHOWER:
             snprintf(weather_str, str_len, "局部阵雨");
             break;
         case TW_WEATHER_FREEZING_RAIN:
             snprintf(weather_str, str_len, "冻雨");
             break;
         case TW_WEATHER_SLEET:
             snprintf(weather_str, str_len, "雨夹雪");
             break;
         case TW_WEATHER_LIGHT_SNOW:
             snprintf(weather_str, str_len, "小雪");
             break;
         case TW_WEATHER_MODERATE_SNOW:
             snprintf(weather_str, str_len, "中雪");
             break;
         case TW_WEATHER_SNOW:
             snprintf(weather_str, str_len, "雪");
             break;
         case TW_WEATHER_HEAVY_SNOW:
             snprintf(weather_str, str_len, "大雪");
             break;
         case TW_WEATHER_LIGHT_TO_MODERATE_SNOW:
             snprintf(weather_str, str_len, "小到中雪");
             break;
         case TW_WEATHER_SNOW_SHOWER:
             snprintf(weather_str, str_len, "阵雪");
             break;
         case TW_WEATHER_LIGHT_SNOW_SHOWER:
             snprintf(weather_str, str_len, "小阵雪");
             break;
         case TW_WEATHER_BLIZZARD:
             snprintf(weather_str, str_len, "暴雪");
             break;
         case TW_WEATHER_HAIL:
             snprintf(weather_str, str_len, "冰雹");
             break;
         case TW_WEATHER_NEEDLE_ICE:
             snprintf(weather_str, str_len, "冰针");
             break;
         case TW_WEATHER_ICE_PELLETS:
             snprintf(weather_str, str_len, "冰粒");
             break;
         case TW_WEATHER_THUNDERSTORM:
             snprintf(weather_str, str_len, "雷暴");
             break;
         case TW_WEATHER_THUNDER_AND_LIGHTNING:
             snprintf(weather_str, str_len, "雷电");
             break;
         case TW_WEATHER_THUNDERSHOWER:
             snprintf(weather_str, str_len, "雷阵雨");
             break;
         case TW_WEATHER_THUNDERSHOWER_AND_HAIL:
             snprintf(weather_str, str_len, "雷阵雨冰雹");
             break;
         case TW_WEATHER_SANDSTORM:
             snprintf(weather_str, str_len, "沙尘暴");
             break;
         case TW_WEATHER_STRONG_SANDSTORM:
             snprintf(weather_str, str_len, "强沙尘暴");
             break;
         case TW_WEATHER_SAND_BLOWING:
             snprintf(weather_str, str_len, "扬沙");
             break;
         case TW_WEATHER_DUST:
             snprintf(weather_str, str_len, "浮尘");
             break;
         case TW_WEATHER_DUST_DEVIL:
             snprintf(weather_str, str_len, "尘卷风");
             break;
         default:
             snprintf(weather_str, str_len, "未知");
             break;
     }
 }

// 获取并更新天气信息
static void __app_weather_update(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    // 检查是否允许更新天气
    if (false == tuya_weather_allow_update()) {
        PR_DEBUG("Weather update not allowed\r\n");
        return;
    }
    
    PR_INFO("Fetching weather data...\r\n");
    
    // 获取当前天气条件
    WEATHER_CURRENT_CONDITIONS_T current = {0};
    rt = tuya_weather_get_current_conditions(&current);
    if (OPRT_OK != rt) {
        PR_ERR("get current conditions failed: %d\r\n", rt);
        return;
    }
    
    // 获取风力信息（中国地区）
    char wind_dir[64] = {0}, wind_speed[64] = {0};
    int wind_level = 0;
    rt = tuya_weather_get_current_wind_cn(wind_dir, wind_speed, &wind_level);
    if (OPRT_OK != rt) {
        PR_WARN("get wind info failed: %d, using default\r\n", rt);
        strcpy(wind_dir, "--");
        wind_level = 0;
    }
    
    // 获取城市信息
    char province[64] = {0}, city[64] = {0}, area[64] = {0};
    rt = tuya_weather_get_city(province, city, area);
    if (OPRT_OK != rt) {
        PR_WARN("get city info failed: %d\r\n", rt);
        strcpy(area, "--");
    }
    
    // 转换天气状态码为字符串
    char status_str[32] = {0};
    weather_code_to_string(current.weather, status_str, sizeof(status_str));
    
    PR_INFO("Weather: %s, temp=%d, humi=%d, wind=%s Lv%d, status=%s\r\n",
            area, current.temp, current.humi, wind_dir, wind_level, status_str);
    
    // 更新 EPD 天气显示
    extern void EPD_7in5_set_weather(int temp, int humidity, const char *wind_dir, 
                                      int wind_level, const char *status, const char *area);
    extern void EPD_7in5_update_weather(void);
    
    EPD_7in5_set_weather(current.temp, current.humi, wind_dir, wind_level, status_str, area);
    EPD_7in5_update_weather();
}

// 天气定时器回调
static void __app_weather_tm_cb(TIMER_ID timer_id, void *arg)
{
    __app_weather_update();
}

static void __app_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
    uint32_t free_heap = tal_system_get_free_heap_size();
    PR_INFO("Free heap size:%d", free_heap);
}

static void __app_display_net_status_update(void)
{
    UI_WIFI_STATUS_E wifi_status = UI_WIFI_STATUS_DISCONNECTED;
    netmgr_status_e net_status = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if (net_status == NETMGR_LINK_UP) {
        // get rssi
        int8_t rssi = 0;
#ifndef PLATFORM_T5
        // BUG: Getting RSSI causes a crash on T5 platform
        tkl_wifi_station_get_conn_ap_rssi(&rssi);
#endif
        if (rssi >= -60) {
            wifi_status = UI_WIFI_STATUS_GOOD;
        } else if (rssi >= -70) {
            wifi_status = UI_WIFI_STATUS_FAIR;
        } else {
            wifi_status = UI_WIFI_STATUS_WEAK;
        }
        static bool  first_time = true;
        if(first_time)
        {
            first_time = false;
         
            // 创建天气更新定时器（30分钟更新一次）
            tal_sw_timer_create(__app_weather_tm_cb, NULL, &system_info.weather_tm);
            tal_sw_timer_start(system_info.weather_tm, WEATHER_UPDATE_TM, TAL_TIMER_CYCLE);
            
            // 延迟10秒后首次获取天气（等待网络连接）
            tal_sw_timer_create(__app_weather_tm_cb, NULL, &system_info.weather_tm);
            tal_sw_timer_start(system_info.weather_tm, 10 * 1000, TAL_TIMER_ONCE);
        }
    } else {
        wifi_status = UI_WIFI_STATUS_DISCONNECTED;
    }

    if (wifi_status != system_info.last_net_status) {
        system_info.last_net_status = wifi_status;
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_NETWORK, (uint8_t *)&wifi_status, sizeof(UI_WIFI_STATUS_E));
#endif
    }
}

// static  void __app_display_status_time_update(TIMER_ID timer_id,  void *arg)
// {
//     POSIX_TM_S tm = {0};
//     tal_time_get_local_time_custom(0, &tm);
//     uint8_t force_update = *(uint8_t *)arg;
//     char tm_str[10] = {0};

//     if (tm.tm_hour != system_info.hour || tm.tm_min != system_info.min || force_update) {
//         system_info.hour = tm.tm_hour;
//         system_info.min = tm.tm_min;

        
//         snprintf(tm_str, sizeof(tm_str), "%02d:%02d", system_info.hour, system_info.min);
//         // extern void EPD_7in5_show_time(char *time_str);
//         //  EPD_7in5_show_time(tm_str);
// #if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
//         app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)tm_str, strlen(tm_str));
// #endif
//     }
//     PR_DEBUG("---------------time_update time:%s-----------------\r\n", tm_str);
// }


static void __app_display_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    static uint32_t net_status_cnt = 0;

    // Update the network status every 10 minutes
    if ((net_status_cnt * DISPLAY_STATUS_TM) >= 1000 || net_status_cnt == 0) {
        __app_display_net_status_update();
        net_status_cnt = 0;
    }
    net_status_cnt++;

}

void app_system_info(void)
{
    // Free heap size
    tal_sw_timer_create(__app_free_heap_tm_cb, NULL, &system_info.heap_tm);
    tal_sw_timer_start(system_info.heap_tm, FREE_HEAP_TM, TAL_TIMER_CYCLE);

    // display status update
    tal_sw_timer_create(__app_display_status_tm_cb, NULL, &system_info.display_status_tm);


    // Set the initial network status
    system_info.last_net_status = UI_WIFI_STATUS_DISCONNECTED;
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_send_msg(TY_DISPLAY_TP_NETWORK, &system_info.last_net_status, sizeof(system_info.last_net_status));
#endif

    // Set the initial status
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)INITIALIZING, strlen(INITIALIZING));
    app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)"NATURAL", strlen("NATURAL"));
#endif

    tal_sw_timer_start(system_info.display_status_tm, DISPLAY_STATUS_TM, TAL_TIMER_CYCLE);
    
    
}

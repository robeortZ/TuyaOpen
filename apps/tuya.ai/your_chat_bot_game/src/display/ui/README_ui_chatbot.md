# ui_chatbot.c - 聊天机器人游戏版 UI 实现

## 文件概述

`ui_chatbot.c` 是聊天机器人游戏版应用的核心 UI 实现文件，基于 LVGL 图形库构建用户界面。该文件实现了聊天机器人界面的初始化、管理和各种显示状态的控制，同时集成了小游戏功能。

## 主要功能

### 🎨 UI 组件管理
- **容器布局**：主容器、状态栏、内容区域
- **聊天界面**：消息显示、表情显示、状态提示
- **游戏集成**：2048 游戏和小恐龙游戏入口
- **状态管理**：网络状态、通知、聊天模式显示

### 🎮 游戏功能
- **2048 游戏**：数字拼图游戏
- **小恐龙游戏**：跑酷类游戏
- **游戏容器**：独立的游戏选择界面

### 🎭 主题系统
- **浅色主题**：默认的明亮配色方案
- **深色主题**：暗色配色方案（预留）
- **动态配色**：支持用户消息、助手消息、系统消息的不同颜色

## 核心数据结构

### APP_THEME_COLORS_T
主题颜色配置结构体：
```c
typedef struct {
    lv_color_t background;        // 背景色
    lv_color_t text;             // 文本色
    lv_color_t chat_background;  // 聊天背景色
    lv_color_t user_bubble;      // 用户消息气泡色
    lv_color_t assistant_bubble; // 助手消息气泡色
    lv_color_t system_bubble;    // 系统消息气泡色
    lv_color_t system_text;      // 系统文本色
    lv_color_t border;           // 边框色
    lv_color_t low_battery;      // 低电量提示色
} APP_THEME_COLORS_T;
```

### APP_UI_T
UI 组件结构体：
```c
typedef struct {
    lv_obj_t *container;         // 主容器
    lv_obj_t *status_bar;        // 状态栏
    lv_obj_t *content;           // 内容区域
    lv_obj_t *emotion_label;     // 表情标签
    lv_obj_t *emotion_image;     // 表情图片
    lv_obj_t *chat_message_label; // 聊天消息标签
    lv_obj_t *status_label;      // 状态标签
    lv_obj_t *network_label;     // 网络状态标签
    lv_obj_t *notification_label; // 通知标签
    lv_obj_t *mute_label;        // 静音标签
    lv_obj_t *chat_mode_label;   // 聊天模式标签
} APP_UI_T;
```

### APP_CHATBOT_UI_T
完整的 UI 管理结构体：
```c
typedef struct {
    APP_UI_T ui;                 // UI 组件
    APP_THEME_COLORS_T theme;    // 主题颜色
    UI_FONT_T font;              // 字体配置
    lv_timer_t *notification_tm; // 通知定时器
} APP_CHATBOT_UI_T;
```

## 主要函数

### 初始化函数

#### `ui_init(UI_FONT_T *ui_font)`
- **功能**：初始化整个 UI 界面
- **参数**：`ui_font` - 字体配置结构体
- **返回值**：成功返回 0，失败返回 -1
- **说明**：创建所有 UI 组件，设置布局和样式

### 消息显示函数

#### `ui_set_user_msg(const char *text)`
- **功能**：显示用户消息
- **参数**：`text` - 消息文本
- **样式**：绿色气泡背景

#### `ui_set_assistant_msg(const char *text)`
- **功能**：显示助手消息
- **参数**：`text` - 消息文本
- **样式**：白色气泡背景

#### `ui_set_system_msg(const char *text)`
- **功能**：显示系统消息
- **参数**：`text` - 消息文本
- **样式**：灰色气泡背景，灰色文本

### 状态管理函数

#### `ui_set_emotion(const char *emotion)`
- **功能**：设置表情显示
- **参数**：`emotion` - 表情名称
- **说明**：支持表情符号显示

#### `ui_set_status(const char *status)`
- **功能**：设置状态栏显示
- **参数**：`status` - 状态文本

#### `ui_set_notification(const char *notification)`
- **功能**：显示通知消息
- **参数**：`notification` - 通知文本
- **特性**：3秒自动消失

#### `ui_set_network(char *wifi_icon)`
- **功能**：设置网络状态图标
- **参数**：`wifi_icon` - WiFi 图标字符

#### `ui_set_chat_mode(const char *chat_mode)`
- **功能**：设置聊天模式显示
- **参数**：`chat_mode` - 聊天模式文本

### 游戏相关函数

#### `ui_btn_game_event_cb(lv_event_t *e)`
- **功能**：2048 游戏按钮事件回调
- **说明**：点击后跳转到 2048 游戏界面

#### `ui_btn_dino_event_cb(lv_event_t *e)`
- **功能**：小恐龙游戏按钮事件回调
- **说明**：点击后跳转到小恐龙游戏界面

## UI 布局设计

### 主界面布局
```
┌─────────────────────────────────────┐
│ 状态栏 (聊天模式 | 状态 | 网络图标)  │
├─────────────────────────────────────┤
│                                     │
│           表情显示区域               │
│                                     │
│        聊天消息显示区域              │
│                                     │
└─────────────────────────────────────┘
┌─────────┐
│ 小游戏  │
│ 玩2048  │
│ 小恐龙  │
└─────────┘
```

### 游戏容器布局
- **位置**：屏幕右侧独立区域
- **尺寸**：160x320 像素
- **背景色**：米色 (#FFEBCD)
- **布局**：垂直排列，居中对齐

## 主题配色方案

### 浅色主题（默认）
- **背景色**：白色 (#FFFFFF)
- **文本色**：黑色 (#000000)
- **用户消息**：绿色 (#95EC69)
- **助手消息**：白色 (#FFFFFF)
- **系统消息**：灰色 (#E0E0E0)
- **边框色**：浅灰色 (#E0E0E0)

### 深色主题（预留）
- **背景色**：深灰色 (#121212)
- **文本色**：白色 (#FFFFFF)
- **用户消息**：深绿色 (#1A6C37)
- **助手消息**：深灰色 (#333333)
- **系统消息**：中灰色 (#2A2A2A)

## 依赖关系

### 外部依赖
- **LVGL**：图形界面库
- **tuya_cloud_types.h**：涂鸦云类型定义
- **ui_display.h**：UI 显示相关头文件
- **font_awesome_symbols.h**：字体图标定义

### 外部函数
- `ui_2048_show()`：2048 游戏界面
- `ui_dino_show()`：小恐龙游戏界面
- `ui_rgb_control_show()`：RGB 控制界面（已注释）

## 编译条件

该文件仅在以下条件满足时编译：
```c
#if defined(ENABLE_GUI_CHATBOT) && (ENABLE_GUI_CHATBOT == 1)
```

## 使用示例

### 基本初始化
```c
UI_FONT_T font_config;
// 配置字体...

ui_init(&font_config);
```

### 显示消息
```c
// 显示用户消息
ui_set_user_msg("你好，我是用户");

// 显示助手回复
ui_set_assistant_msg("你好！我是 AI 助手，很高兴为您服务！");

// 显示系统消息
ui_set_system_msg("系统正在初始化...");
```

### 设置状态
```c
// 设置表情
ui_set_emotion("HAPPY");

// 设置状态
ui_set_status("正在聆听...");

// 显示通知
ui_set_notification("网络连接成功");

// 设置网络状态
ui_set_network("📶");
```

## 注意事项

1. **内存管理**：所有 UI 对象由 LVGL 自动管理
2. **线程安全**：UI 操作应在主线程中进行
3. **字体依赖**：需要正确配置字体文件
4. **屏幕适配**：布局基于 LV_VER_RES 和 LV_HOR_RES 宏
5. **游戏集成**：游戏功能需要对应的游戏模块支持

## 扩展建议

1. **添加新游戏**：在游戏容器中添加新的游戏按钮
2. **自定义主题**：实现更多主题配色方案
3. **动画效果**：为 UI 切换添加动画过渡
4. **多语言支持**：支持不同语言的界面文本
5. **响应式布局**：适配不同尺寸的屏幕

---

**版权信息**：Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.



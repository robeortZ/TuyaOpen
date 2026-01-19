# TuyaOpen - AI+IoT 开发框架

<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/c128362b-eb25-4512-b5f2-ad14aae2395c.jpg" width="100%" >
</p>

<p align="center">
  <a href="https://tuyaopen.ai/docs/quick_start/enviroment-setup">快速开始</a> ·
  <a href="https://developer.tuya.com/cn/docs/iot/ai-agent-management?id=Kdxr4v7uv4fud">涂鸦 AI Agent</a> ·
  <a href="https://tuyaopen.ai/docs/about-tuyaopen">文档中心</a> ·
  <a href="https://tuyaopen.ai/docs/hardware-specific/t5-ai-board/overview-t5-ai-board">硬件资源</a>
</p>

<p align="center">
    <a href="https://tuyaopen.ai" target="_blank">
        <img alt="Static Badge" src="https://img.shields.io/badge/Product-F04438"></a>
    <a href="https://tuyaopen.ai/pricing" target="_blank">
        <img alt="Static Badge" src="https://img.shields.io/badge/free-pricing?logo=free&color=%20%23155EEF&label=pricing&labelColor=%20%23528bff"></a>
    <a href="https://discord.gg/cbGrBjx7" target="_blank">
        <img src="https://img.shields.io/badge/Discord-Join%20Chat-5462eb?logo=discord&labelColor=%235462eb&logoColor=%23f5f5f5&color=%235462eb"
            alt="chat on Discord"></a>
    <a href="https://www.youtube.com/@tuya2023" target="_blank">
        <img src="https://img.shields.io/badge/YouTube-Subscribe-red?logo=youtube&labelColor=white"
            alt="Subscribe on YouTube"></a>
    <a href="https://x.com/tuyasmart" target="_blank">
        <img src="https://img.shields.io/twitter/follow/tuyasmart?logo=X&color=%20%23f5f5f5"
            alt="follow on X(Twitter)"></a>
    <a href="https://www.linkedin.com/company/tuya-smart/" target="_blank">
        <img src="https://custom-icon-badges.demolab.com/badge/LinkedIn-0A66C2?logo=linkedin-white&logoColor=fff"
            alt="follow on LinkedIn"></a>
    <a href="https://github.com/tuya/tuyaopen/graphs/commit-activity?branch=dev" target="_blank">
        <img alt="Commits last month (dev branch)" src="https://img.shields.io/github/commit-activity/m/tuya/tuyaopen/dev?labelColor=%2332b583&color=%2312b76a"></a>
    <a href="https://github.com/langgenius/dify/" target="_blank">
        <img alt="Issues closed" src="https://img.shields.io/github/issues-search?query=repo%3Atuya%2Ftuyaopen%20is%3Aclosed&label=issues%20closed&labelColor=%20%237d89b0&color=%20%235d6b98"></a>
</p>

<p align="center">
  <a href="./README.md"><img alt="README in English" src="https://img.shields.io/badge/English-d9d9d9"></a>
  <a href="./README_zh.md"><img alt="简体中文版自述文件" src="https://img.shields.io/badge/简体中文-d9d9d9"></a>
</p>

## 概述

TuyaOpen 是一个开源的 AI+IoT 开发框架，旨在帮助开发者快速创建智能互联设备。它支持多种芯片平台和类 RTOS 操作系统，能够无缝集成多模态 AI 能力，包括音频、视频和传感器数据处理。

### 🚀 核心功能

- **语音技术**：支持 `ASR`（语音识别）、`KWS`（关键词唤醒）、`TTS`（语音合成）、`STT`（语音转文本）
- **AI 集成**：集成主流 LLMs 及 AI 平台，包括 `Deepseek`、`ChatGPT`、`Claude`、`Gemini` 等
- **多模态 AI**：构建具备文本、语音、视觉和基于传感器的智能设备
- **云端连接**：无缝连接至涂鸦云，实现远程控制、监控和 OTA 升级
- **智能家居**：开发兼容 `Google Home` 和 `Amazon Alexa` 的设备
- **硬件支持**：支持广泛的硬件应用，包括 `蓝牙`、`Wi-Fi`、`以太网` 等多种连接方式
- **安全可靠**：内置强大的安全性、设备认证和数据加密能力

### TuyaOpen SDK 框架
<p align="center">
<img src="https://images.tuyacn.com/fe-static/docs/img/25713212-9840-4cf5-889c-6f55476a59f9.jpg" width="80%" >
</p>

---

## 支持的目标平台

| 平台名称 | 支持状态 | 介绍 | 调试串口 |
|---------|---------|------|---------|
| Ubuntu | 支持 | 可直接运行于如 ubuntu 等 Linux 主机 | - |
| Tuya T2 | 支持 | 支持的模块列表: [T2-U](https://developer.tuya.com/en/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq) | Uart2/115200 |
| Tuya T3 | 支持 | 支持的模块列表: [T3-U](https://developer.tuya.com/en/docs/iot/T3-U-Module-Datasheet?id=Kdd4pzscwf0il) [T3-U-IPEX](https://developer.tuya.com/en/docs/iot/T3-U-IPEX-Module-Datasheet?id=Kdn8r7wgc24pt) [T3-2S](https://developer.tuya.com/en/docs/iot/T3-2S-Module-Datasheet?id=Ke4h1uh9ect1s) [T3-3S](https://developer.tuya.com/en/docs/iot/T3-3S-Module-Datasheet?id=Kdhkyow9fuplc) [T3-E2](https://developer.tuya.com/en/docs/iot/T3-E2-Module-Datasheet?id=Kdirs4kx3uotg) 等 | Uart1/460800 |
| Tuya T5 | 支持 | 支持的模块列表: [T5-E1](https://developer.tuya.com/en/docs/iot/T5-E1-Module-Datasheet?id=Kdar6hf0kzmfi) [T5-E1-IPEX](https://developer.tuya.com/en/docs/iot/T5-E1-IPEX-Module-Datasheet?id=Kdskxvxe835tq) 等 | Uart1/460800 |
| ESP32/ESP32C3/ESP32S3 | 支持 | 支持多种 ESP32 系列芯片 | Uart0/115200 |
| LN882H | 支持 | 支持 LN882H 芯片平台 | Uart1/921600 |
| BK7231N | 支持 | 支持的模块列表: [CBU](https://developer.tuya.com/en/docs/iot/cbu-module-datasheet?id=Ka07pykl5dk4u) [CB3S](https://developer.tuya.com/en/docs/iot/cb3s?id=Kai94mec0s076) [CB3L](https://developer.tuya.com/en/docs/iot/cb3l-module-datasheet?id=Kai51ngmrh3qm) [CB3SE](https://developer.tuya.com/en/docs/iot/CB3SE-Module-Datasheet?id=Kanoiluul7nl2) [CB2S](https://developer.tuya.com/en/docs/iot/cb2s-module-datasheet?id=Kafgfsa2aaypq) [CB2L](https://developer.tuya.com/en/docs/iot/cb2l-module-datasheet?id=Kai2eku1m3pyl) [CB1S](https://developer.tuya.com/en/docs/iot/cb1s-module-datasheet?id=Kaij1abmwyjq2) [CBLC5](https://developer.tuya.com/en/docs/iot/cblc5-module-datasheet?id=Ka07iqyusq1wm) [CBLC9](https://developer.tuya.com/en/docs/iot/cblc9-module-datasheet?id=Ka42cqnj9r0i5) [CB8P](https://developer.tuya.com/en/docs/iot/cb8p-module-datasheet?id=Kahvig14r1yk9) 等 | Uart2/115200 |

---

## 应用示例

### 🤖 AI 智能应用

#### 1. 聊天机器人 (your_chat_bot)
基于 tuya.ai 的开源大模型智能聊天机器人，支持语音交互和实时显示。

**主要功能：**
- AI 智能对话
- 按键唤醒/语音唤醒，回合制对话
- 表情显示
- LCD 显示实时聊天内容
- 蓝牙配网快捷连接
- APP 端实时切换 AI 智能体角色

**支持硬件：**
- TUYA T5AI_Board 开发板
- TUYA T5AI_EVB 开发板
- 正点原子 ESP32S3BOX
- waveshare ESP32S3 触摸 AMOLED 开发板
- 星智 ESP32S3 0.96 OLED 开发板

#### 2. 聊天机器人游戏版 (your_chat_bot_game)
在聊天机器人基础上增加了小游戏功能，包括 2048 游戏和小恐龙游戏。

**新增功能：**
- 2048 数字游戏
- 小恐龙跑酷游戏
- 游戏与 AI 对话的无缝切换

#### 3. T5 Pocket AI 宠物
便携式 AI 语音和视觉大语言模型应用设备。

**产品亮点：**
- Tuya T5 Wi-Fi & 蓝牙模块
- 2.9" 单色低功耗 LCD 屏幕
- 4G CAT.1 支持
- BMI270 6轴 IMU 传感器
- 内置 1 通道扬声器和 2 通道麦克风
- 摇杆和按键控制
- DVP 摄像头
- USB Type-C 接口

**AI 宠物功能：**
- 虚拟宠物体验
- 音频、视觉和 LLM 功能
- 自然语音对话
- 情感感知
- 状态管理（饥饿、快乐、清洁、健康）
- 时间同步和持久化存储

#### 4. 表情聊天机器人 (chat_bot_emoji_fs)
支持表情符号和文件系统的聊天机器人版本。

#### 5. 双眼神情 (duo_eye_mood)
具有双眼神情显示功能的 AI 设备。

#### 6. Otto 机器人 (your_otto_robot)
基于 Otto 机器人的 AI 交互应用。

### 🌐 云端应用

#### 1. 开关演示 (switch_demo)
简单的跨平台开关示例，支持多种连接方式。

**功能特点：**
- 远程控制（通过涂鸦 APP）
- 局域网控制（同一 LAN 内）
- 蓝牙控制（无网络时）
- 设备配对和激活
- OTA 升级

#### 2. 天气获取演示 (weather_get_demo)
演示如何从涂鸦云获取天气信息。

#### 3. 摄像头演示 (camera_demo)
展示视频流处理和云端传输功能。

### 🎮 游戏应用

#### 游戏集合 (games)
包含各种有趣的游戏应用示例，展示如何在 TuyaOpen 平台上开发创意和引人入胜的应用。

---

## 快速开始

### 1. 环境准备

```bash
# 克隆项目
git clone https://github.com/tuya/TuyaOpen.git
cd TuyaOpen

# 安装依赖
pip install -r requirements.txt
```

### 2. 选择目标平台

```bash
# 进入应用目录
cd apps/tuya.ai/your_chat_bot

# 选择开发板
tos.py config choice
```

### 3. 配置项目

```bash
# 修改配置（可选）
tos.py config menu
```

### 4. 编译和烧录

```bash
# 编译项目
tos.py build

# 烧录到设备
tos.py flash
```

### 5. 获取授权码

**方式1：** 购买已烧录 TuyaOpen 授权码的模块

**方式2：** 通过 [涂鸦平台](https://platform.tuya.com/purchase/index?type=6) 购买 TuyaOpen 授权码

**方式3：** 在 `tuya_config.h` 文件中替换以下内容：
```c
#define TUYA_DEVICE_UUID "your_uuid_here"
#define TUYA_DEVICE_AUTHKEY "your_authkey_here"
```

---

## 项目结构

```
TuyaOpen/
├── apps/                    # 应用示例
│   ├── tuya.ai/            # AI 相关应用
│   │   ├── your_chat_bot/  # 聊天机器人
│   │   ├── your_chat_bot_game/  # 聊天机器人游戏版
│   │   ├── your_otto_robot/     # Otto 机器人
│   │   └── ...
│   ├── tuya_cloud/         # 云端应用
│   │   ├── switch_demo/    # 开关演示
│   │   ├── weather_get_demo/   # 天气获取演示
│   │   └── camera_demo/    # 摄像头演示
│   ├── games/              # 游戏应用
│   └── tuya_t5_pocket/     # T5 Pocket 设备
├── src/                    # 源代码
├── platform/              # 平台支持
├── boards/                # 开发板配置
├── tools/                 # 开发工具
├── examples/              # 示例代码
└── docs/                  # 文档
```

---

## 开发指南

### 创建新应用

1. 在 `apps/` 目录下创建新的应用文件夹
2. 复制现有应用作为模板
3. 修改 `CMakeLists.txt` 和配置文件
4. 实现应用逻辑

### 添加新硬件支持

1. 在 `platform/` 目录下添加平台支持
2. 在 `boards/` 目录下添加开发板配置
3. 更新 CMakeLists.txt 文件

### 配置系统

使用 `tos.py config menu` 命令可以配置：
- 对话模式（按键、唤醒、随意对话）
- 唤醒词选择
- AEC 回声消除
- 显示配置
- 硬件引脚配置

---

## 贡献指南

我们欢迎社区贡献！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request

更多信息请参考 [贡献指南](https://tuyaopen.ai/docs/contribute/contribute-guide)。

---

## 相关链接

- **官方文档**: [https://tuyaopen.ai/docs/about-tuyaopen](https://tuyaopen.ai/docs/about-tuyaopen)
- **Arduino 版本**: [https://github.com/tuya/arduino-TuyaOpen](https://github.com/tuya/arduino-TuyaOpen)
- **Luanode 版本**: [https://github.com/tuya/luanode-TuyaOpen](https://github.com/tuya/luanode-TuyaOpen)
- **涂鸦开发者平台**: [https://developer.tuya.com](https://developer.tuya.com)
- **涂鸦 IoT 平台**: [https://iot.tuya.com](https://iot.tuya.com)

---

## 许可证

本项目基于 Apache License Version 2.0 发布。更多信息请参见 [LICENSE](LICENSE) 文件。

---

## 免责声明

用户需明确知晓，本项目可能包含由第三方开发的子模块。这些子模块可能会独立于本项目进行更新。鉴于这些子模块的更新频率不可控，本项目无法保证其始终为最新版本。因此，若用户在使用本项目过程中遇到与子模块相关的问题，建议根据需要自行更新，或向本项目提交 issue。

如用户决定将本项目用于商业用途，应充分认识到其中可能存在的功能和安全风险。在此情况下，用户应对所有功能和安全问题自行承担责任，并进行全面的功能和安全性测试，以确保其满足特定业务需求。本公司不对因用户使用本项目或其子模块而导致的任何直接、间接、特殊、偶发或惩罚性损害承担责任。

---

## 社区支持

- **Discord**: [https://discord.gg/cbGrBjx7](https://discord.gg/cbGrBjx7)
- **YouTube**: [https://www.youtube.com/@tuya2023](https://www.youtube.com/@tuya2023)
- **Twitter**: [https://x.com/tuyasmart](https://x.com/tuyasmart)
- **LinkedIn**: [https://www.linkedin.com/company/tuya-smart/](https://www.linkedin.com/company/tuya-smart/)

---

<p align="center">
<strong>让 AI+IoT 开发变得更简单！</strong>
</p>



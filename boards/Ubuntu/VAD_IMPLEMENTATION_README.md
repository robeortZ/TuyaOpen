# Ubuntu平台 VAD (语音活动检测) 实现记录

## 概述

本文档记录了在树莓派(Ubuntu平台)上实现VAD功能所涉及的所有文件修改。

**实现日期**: 2025-12-08  
**功能**: 基于能量的语音活动检测，支持自适应噪声底、语音打断等特性

---

## 修改文件列表

### 1. 新增文件

#### `/home/tuya/TuyaOpen/boards/Ubuntu/tkl_vad.c`
**用途**: VAD核心算法实现

**功能特性**:
- 基于RMS能量的语音检测
- 自适应噪声底估计
- 语音起始/结束迟滞处理
- 可调参数减少误识别

**关键参数** (可调整):
```c
#define VAD_ENERGY_THRESHOLD_LOW    60      // 最小能量阈值 (增大减少误触发)
#define VAD_ENERGY_THRESHOLD_HIGH   300     // 高能量阈值
#define VAD_SPEECH_HANGOVER_FRAMES  50      // 语音结束后保持帧数 (500ms)
#define VAD_SPEECH_START_FRAMES     3       // 语音开始需要的连续高能量帧数
```

**主要函数**:
- `tkl_vad_init()` - 初始化VAD
- `tkl_vad_start()` - 启动VAD检测
- `tkl_vad_stop()` - 停止VAD检测
- `tkl_vad_process()` - 处理音频数据
- `tkl_vad_get_status()` - 获取当前VAD状态

---

### 2. 修改文件

#### `/home/tuya/TuyaOpen/boards/Ubuntu/CMakeLists.txt`
**修改内容**: 添加 `tkl_vad.c` 到编译列表

```cmake
set(LIB_SRCS
    ${MODULE_PATH}/board_com_api.c
    ${MODULE_PATH}/keyboard_input.c
    ${MODULE_PATH}/tkl_vad.c        # 新增
)
```

---

#### `/home/tuya/TuyaOpen/platform/Ubuntu/tuyaos_adapter/include/tkl_vad.h`
**修改内容**: 删除原有的 `static inline` 桩实现，替换为正确的函数声明头文件

**原因**: 原文件包含空的内联函数实现，会覆盖真正的VAD实现

**操作**: 从 `tools/porting/adapter/vad/tkl_vad.h` 复制正确的头文件

---

#### `/home/tuya/TuyaOpen/apps/tuya.ai/your_chat_bot/app_default.config`
**修改内容**: 启用VAD自由对话模式

```
# CONFIG_ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL=y  # 注释掉
CONFIG_ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE=y        # 启用VAD模式
```

---

#### `/home/tuya/TuyaOpen/apps/tuya.ai/ai_components/ai_audio/src/ai_audio_input.c`
**修改内容**: 

1. **添加头文件引用**:
```c
#include "tuya_ai_client.h"
```

2. **AI Client就绪检查** (防止启动时上传失败):
```c
if (event == AI_AUDIO_INPUT_EVT_GET_VALID_VOICE_START && !tuya_ai_client_is_ready()) {
    PR_DEBUG("AI client not ready, skip VOICE_START event");
    sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
} else {
    sg_audio_input_inform_cb(event, NULL);
}
```

3. **语音打断功能** (用户说话时停止AI播放):
```c
// Always keep VAD running to detect voice interruption
tkl_vad_start();

if (true == ai_audio_player_is_playing()) {
    // Feed data to VAD to check for speech
    if (true == sg_audio_input.is_enable_get_valid_data) {
        __ai_audio_detect_valid_data_feed(sg_audio_input.method, (uint8_t *)data, len);
    }
    
    // If VAD detects speech, interrupt the playback
    if (TKL_VAD_STATUS_SPEECH == tkl_vad_get_status()) {
        PR_NOTICE("Voice interrupt detected! Stopping AI playback...");
        ai_audio_player_stop();
        voice_interrupt = true;
    } else {
        return;
    }
}
```

---

## 功能说明

### VAD工作模式

启用 `CONFIG_ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE=y` 后:

1. **按S键触发唤醒** → 系统开始监听
2. **VAD自动检测语音** → 检测到说话开始录音
3. **VAD检测到静音** → 自动结束录音并上传
4. **AI回复时可打断** → 用户说话会停止AI播放

### 参数调整指南

| 问题 | 调整方案 |
|------|----------|
| 误识别太多(噪音→语音) | 增大 `VAD_ENERGY_THRESHOLD_LOW`: 60→80→100 |
| 需要更响声音才触发 | 增大 `VAD_SPEECH_START_FRAMES`: 3→5→8 |
| 语音结束太快被截断 | 增大 `VAD_SPEECH_HANGOVER_FRAMES`: 50→80 |
| 漏检太多(说话没反应) | 减小 `VAD_ENERGY_THRESHOLD_LOW`: 60→40→30 |

### 调试日志

运行时观察以下日志:
```
VAD initialized: sample_rate=16000, scale=1.00, threshold=150.0
VAD started: threshold=XX.X, noise_floor=XX.X
VAD: energy=XX.X, threshold=XX.X, noise=XX.X, status=SPEECH/NONE
VAD: >>> Speech STARTED (energy=XX.X, threshold=XX.X)
VAD: <<< Speech ENDED (silence_frames=XX)
Voice interrupt detected! Stopping AI playback...
```

---

## 编译命令

```bash
cd /home/tuya/TuyaOpen/apps/tuya.ai/your_chat_bot
source /home/tuya/TuyaOpen/.venv/bin/activate
python3 /home/tuya/TuyaOpen/tos.py build
```

## 运行命令

```bash
cd /home/tuya/TuyaOpen/apps/tuya.ai/your_chat_bot/dist/your_chat_bot_1.0.1
./your_chat_bot_QIO_1.0.1.bin
```

---

## 文件变更汇总

| 文件 | 操作 | 说明 |
|------|------|------|
| `boards/Ubuntu/tkl_vad.c` | 新增 | VAD核心算法实现 |
| `boards/Ubuntu/CMakeLists.txt` | 修改 | 添加tkl_vad.c到编译 |
| `platform/Ubuntu/.../tkl_vad.h` | 替换 | 删除桩实现，使用正确头文件 |
| `apps/.../app_default.config` | 修改 | 启用VAD模式 |
| `ai_components/.../ai_audio_input.c` | 修改 | AI就绪检查+语音打断 |

---

## 版本历史

- **v1.0** (2025-12-08): 初始VAD实现
  - 基于能量的语音检测
  - 自适应噪声底
  - 语音打断功能
  - AI Client就绪检查





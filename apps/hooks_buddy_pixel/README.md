# Buddy Pixel

Claude Desktop 的像素屏伴侣，运行在 **TUYA_T5AI_PIXEL** 开发板上。  
通过 WebSocket 连接到 PC 端桥接服务（buddy_bridge），实时在 32×32 LED 矩阵上播放动画，反映 Claude 的工作状态。

---

## 系统架构

```
Claude Code / Claude Desktop
        │
        │ (Claude Code Hooks / BLE)
        ▼
  buddy_bridge (PC 端 Python 服务)
        │
        │ WebSocket  ws://设备IP:8765/device
        ▼
  TUYA_T5AI_PIXEL 开发板
  (32×32 LED 矩阵显示动画)
```

---

## 动画状态对照表

| 状态 | 触发条件 | 播放动画 |
|------|----------|----------|
| DISCONNECTED | 未连接到桥接服务 | clawd-sleeping |
| IDLE | 已连接，Claude 空闲 | clawd-idle |
| BUSY | Claude 正在处理（循环 7 个） | building / carrying / conducting / debugger / sweeping / thinking / typing |
| ATTENTION | 等待用户审批权限请求 | clawd-notification（蜂鸣器同步报警） |
| CELEBRATE | 会话完成 | clawd-happy / clawd-juggling（交替） |
| DIZZY | 长按 B 键摇晃触发 | clawd-conducting |

---

## 硬件按键

| 按键 | 操作 | 功能 |
|------|------|------|
| OK（button1） | 单击 | **批准**权限请求 |
| OK（button1） | 长按 | **清除 WiFi 配置**并重启（重新进入配置模式） |
| A（button2） | 单击 | 导航（发送 navigate 事件） |
| B（button3） | 单击 | **拒绝**权限请求 |
| B（button3） | 长按 | 触发眩晕动画 |

---

## 一、编译固件

### 前置条件

- Ubuntu 20.04 / 22.04（WSL2 同样可用）
- Python 3.8+
- TuyaOpen SDK 已克隆到本地

### 编译步骤

```bash
cd apps/buddy_pixel
python3 ../../tos.py build
```

编译产物位于：

```
apps/buddy_pixel/dist/buddy_pixel_1.0.0/
├── buddy_pixel_QIO_1.0.0.bin   # 全量烧录固件
├── buddy_pixel_UA_1.0.0.bin    # OTA 升级包
└── buddy_pixel_UG_1.0.0.bin    # 差分升级包
```

### 配置默认参数（可选）

在 `app_default.config` 中修改默认值（首次烧录后可通过配置网页覆盖）：

```ini
CONFIG_BUDDY_WIFI_SSID="your_wifi_ssid"
CONFIG_BUDDY_WIFI_PASSWORD="your_wifi_paswd"
CONFIG_BUDDY_BRIDGE_HOST="192.168.1.100"
CONFIG_BUDDY_BRIDGE_PORT=8765
```

---

## 二、烧录固件

使用涂鸦烧录工具（BKFIL / tyutool）烧录 `buddy_pixel_QIO_1.0.0.bin`。

---

## 三、首次配置（配置网页）

首次烧录后，设备找不到 WiFi 配置会自动进入**配置模式**：

1. 手机或电脑搜索 WiFi，连接到热点 **`BuddyPixel-Setup`**（无密码）
2. 浏览器访问 **`http://192.168.4.1`**
3. 填写以下信息后点击「保存配置并重启」：
   - WiFi 名称（SSID）
   - WiFi 密码
   - 桥接服务器 IP（运行 buddy_bridge 的电脑 IP）
   - 端口（默认 8765）
4. 设备自动重启，连接到填写的 WiFi，蜂鸣器发出启动音即表示成功

> **重新配置**：调用 `buddy_config_clear()` 或重新烧录固件后，下次启动会再次进入配置模式。

---

## 四、运行 PC 端桥接服务（buddy_bridge）

桥接服务运行在 **Windows / macOS / Linux** 上，负责：
- 接收 Claude Desktop 的 BLE 状态通知
- 接收 Claude Code Hooks 的状态推送
- 通过 WebSocket 将状态转发给设备

### 安装依赖

```bash
cd tools/buddy_bridge
pip install -r requirements.txt
```

### 启动服务

```bash
# 默认绑定 0.0.0.0:8765，自动扫描 BLE
python server.py

# 不使用 BLE（仅用 Claude Code Hooks 或手动测试）
python server.py --no-ble

# 指定端口
python server.py --port 9000
```

启动后访问控制台：**`http://localhost:8765`**

控制台功能：
- 实时查看设备连接状态和当前动画
- 手动切换所有状态（DISCONNECTED / IDLE / BUSY / **ATTENTION** / CELEBRATE / DIZZY）
- 模拟权限请求（输入提示文字后点击「触发权限请求」）
- 模拟 BLE 消息（运行中 / 等待审批 / 已完成）
- 从控制台点击「批准 / 拒绝」按钮（通过 WebSocket 转发给设备）
- 事件日志

---

## 五、Claude Code 自动状态推送（Hooks）

直连 BLE 方案，**无需** buddy_bridge 服务，无需 WiFi 配置。

### 安装

```bash
pip install bleak
```

### 首次扫描（缓存设备地址）

```bash
python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py scan
```

成功后设备地址会被缓存到 `~/.buddy_pixel_addr`，后续连接无需重新扫描（< 0.5s）。

### 配置 Hooks

编辑 `~/.claude/settings.json`，将路径替换为实际绝对路径：

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": ".*",
        "hooks": [{"type": "command",
          "command": "python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py busy"}]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [{"type": "command",
          "command": "python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py celebrate"}]
      }
    ],
    "Notification": [
      {
        "matcher": "",
        "hooks": [{"type": "command",
          "command": "python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py attention"}]
      }
    ]
  }
}
```

### 可选：用设备按键审批工具调用

把 `PreToolUse` 改为 `approve` 模式，Claude 调用指定工具时设备会显示 ATTENTION 动画并等待按键：
- **OK 键**：允许工具执行
- **B 键**：拒绝（Claude Code 会取消本次工具调用）

```json
"PreToolUse": [
  {
    "matcher": "Bash|Write|Edit",
    "hooks": [{"type": "command",
      "command": "python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py approve"}]
  }
]
```

### Hooks 状态映射

| Hook | 命令 | 触发时机 | 像素屏状态 |
|------|------|----------|------------|
| PreToolUse | `busy` | Claude 开始调用工具 | BUSY |
| PreToolUse | `approve` | 需要设备确认的工具（可选） | ATTENTION + 等待按键 |
| Stop | `celebrate` | Claude 完成本轮回复 | CELEBRATE |
| Notification | `attention` | 通知或权限提示 | ATTENTION |

### 调试

```bash
# 查看 BLE 通信日志
BUDDY_DEBUG=1 python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py busy

# 清除缓存地址（设备更换时使用）
python3 /绝对路径/TuyaOpen/tools/buddy_hook/buddy_hook.py clear
```

---

## 六、添加新 GIF 动画

```bash
# 1. 将 GIF 文件放到 gif/ 目录
# 2. 转换为 C 数组
cd apps/buddy_pixel
python3 tools/gif_to_c.py gif/clawd-newname.gif -o src/claude_status_gif/gif_clawd_newname.c

# 3. 在 buddy_fsm.c 中添加 include 并注册到对应状态

# 4. 重新编译
python3 ../../tos.py build
```

转换参数说明：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--brightness` / `-b` | `1.0` | 亮度系数（固件已做 5% 缩放，此处保持 1.0） |
| `--saturation` / `-S` | `2.0` | 饱和度增强（补偿 LED 低驱动下的色偏） |
| `--size` / `-s` | `32` | 输出尺寸（像素，对应 32×32 矩阵） |

---

## 七、故障排除

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 上电后出现 **BuddyPixel-Setup** 热点 | 首次烧录，无 WiFi 配置 | 连接热点，访问 http://192.168.4.1 配置 |
| 想更换 WiFi 或桥接 IP | 需要重新配置 | **长按 OK 键** 3 秒，蜂鸣器响三声后自动清除配置并重启进入配置模式 |
| 设备显示 sleeping 动画不变 | 未连接到桥接服务 | 确认 buddy_bridge 已启动，检查 IP/端口 |
| 设备频繁断开重连（约 1 分钟一次） | 网络空闲超时 | 正常，buddy_bridge 已有 20 s keepalive，60 s 自动重连 |
| 控制台显示「设备离线」 | 设备 WebSocket 断开 | 等待 5 秒自动重连，或检查网络 |
| 颜色偏暗或偏粉 | GIF 未用新版 gif_to_c.py 转换 | 使用 `--brightness 1.0 --saturation 2.0` 重新转换 |
| 启动崩溃（Usage Fault / STKOF） | 栈溢出 | 确认 `tuya_app` 线程栈已设置为 8 KB |
| 批准/拒绝按钮无响应 | BLE 未连接 | 无 BLE 时设备按键仍可用；控制台按钮需桥接服务在线 |

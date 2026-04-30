#!/usr/bin/env python3
"""
buddy_hook.py — Claude Code → BuddyPixel 直连 BLE

单文件，同时包含后台 daemon 和 hook 客户端。
daemon 维持持久 BLE 连接，hook 命令通过本地 TCP socket 瞬间完成。

安装:
    pip install bleak

启动 daemon (后台，设备始终保持 BLE 连接):
    python3 buddy_hook.py daemon          # 前台运行（调试）
    python3 buddy_hook.py daemon --bg     # 后台运行

hook 命令（首次调用自动启动 daemon）:
    busy        → BUSY 状态
    celebrate   → CELEBRATE 状态（8 秒后自动回 IDLE）
    idle        → IDLE 状态
    attention   → ATTENTION 状态，保持连接等待按键
    approve     → ATTENTION + 等待按键，B 键返回 block 决定给 Claude Code

管理命令:
    scan        扫描并缓存设备地址
    clear       清除缓存地址
    status      查看 daemon 状态
    stop        停止 daemon

~/.claude/settings.json hooks 配置:
    "PreToolUse":  [{"matcher":".*", "hooks":[{"type":"command",
                     "command":"python3 /path/buddy_hook.py busy"}]}]
    "Stop":        [{"matcher":"",   "hooks":[{"type":"command",
                     "command":"python3 /path/buddy_hook.py celebrate"}]}]
    "Notification":[{"matcher":"",   "hooks":[{"type":"command",
                     "command":"python3 /path/buddy_hook.py attention"}]}]

环境变量:
    BUDDY_DEBUG=1   输出调试信息到 stderr
    BUDDY_PORT=18765  daemon 监听端口（默认 18765）
"""

from __future__ import annotations
import asyncio
import json
import os
import socket
import sys
import time
import uuid
from pathlib import Path

# ─── 配置 ────────────────────────────────────────────────────────────────────

DEVICE_NAME     = "BuddyPixel"
WRITE_CHAR      = "00000001-0000-1001-8001-00805f9b07d0"
NOTIFY_CHAR     = "00000002-0000-1001-8001-00805f9b07d0"

ADDR_CACHE      = Path.home() / ".buddy_pixel_addr"
DAEMON_HOST     = "127.0.0.1"
DAEMON_PORT     = int(os.environ.get("BUDDY_PORT", "18765"))
SCAN_TIMEOUT    = 10.0
CONN_TIMEOUT    = 6.0
APPROVE_TIMEOUT = 30       # 等待设备按键的超时（秒）
RECONNECT_DELAY = 5        # BLE 断开后重连等待（秒）

DEBUG = os.environ.get("BUDDY_DEBUG", "0") == "1"


def _dbg(msg: str) -> None:
    if DEBUG:
        print(f"[buddy] {msg}", file=sys.stderr)


# ─── stdin 读取 ───────────────────────────────────────────────────────────────

def _read_stdin() -> dict:
    try:
        if not sys.stdin.isatty():
            data = sys.stdin.read()
            if data.strip():
                return json.loads(data)
    except Exception:
        pass
    return {}


# ═══════════════════════════════════════════════════════════════════════════════
# DAEMON
# ═══════════════════════════════════════════════════════════════════════════════

class BuddyDaemon:
    def __init__(self):
        self._client = None           # BleakClient
        self._connected = False
        self._pending: dict[str, asyncio.Future] = {}  # action_id → Future
        self._auto_approved: set = set()  # tool names approved for this session

    # ── BLE 连接维护 ──────────────────────────────────────────────────────────

    async def _ble_loop(self):
        _dbg("BLE loop started")
        while True:
            try:
                await self._connect_and_maintain()
            except Exception as e:
                _dbg(f"BLE error: {e}")
            self._connected = False
            self._client = None
            _dbg(f"BLE disconnected, retrying in {RECONNECT_DELAY}s …")
            await asyncio.sleep(RECONNECT_DELAY)

    async def _connect_and_maintain(self):
        from bleak import BleakClient, BleakScanner

        address = ADDR_CACHE.read_text().strip() if ADDR_CACHE.exists() else None
        if not address:
            _dbg(f"scanning for {DEVICE_NAME} …")
            device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=SCAN_TIMEOUT)
            if not device:
                _dbg(f"{DEVICE_NAME} not found")
                return
            address = device.address
            ADDR_CACHE.write_text(address)

        _dbg(f"connecting to {address} …")
        async with BleakClient(address,
                               timeout=CONN_TIMEOUT,
                               disconnected_callback=self._on_ble_disconnect) as c:
            self._client = c
            self._connected = True
            await c.start_notify(NOTIFY_CHAR, self._on_notify)
            _dbg(f"BLE connected: {address}")
            while c.is_connected:
                await asyncio.sleep(1.0)

    def _on_ble_disconnect(self, _client):
        _dbg("BLE disconnected callback")
        self._connected = False
        self._client = None

    def _on_notify(self, _sender, data: bytearray):
        try:
            evt = json.loads(data.decode())
            if evt.get("type") != "event":
                return
            action_id = evt.get("action_id", "")
            event_name = evt.get("event", "")
            _dbg(f"notify: event={event_name} action_id={action_id}")
            f = self._pending.get(action_id)
            if f and not f.done() and event_name in ("approve", "deny", "navigate"):
                # call_soon_threadsafe is safe whether the callback fires from
                # within the event loop or from a background bleak thread.
                loop = asyncio.get_event_loop()
                loop.call_soon_threadsafe(f.set_result, event_name)
        except Exception as e:
            _dbg(f"_on_notify error: {e}")

    # ── 发送状态到设备 ────────────────────────────────────────────────────────

    async def send_state(self, msg: dict) -> bool:
        if not self._connected or self._client is None:
            _dbg("send_state: not connected")
            return False
        try:
            payload = (json.dumps(msg) + "\n").encode()
            await self._client.write_gatt_char(WRITE_CHAR, payload, response=False)
            _dbg(f"→ {msg}")
            return True
        except Exception as e:
            _dbg(f"send_state error: {e}")
            return False

    # ── 等待设备按键 ──────────────────────────────────────────────────────────

    async def wait_button(self, action_id: str, timeout: int = APPROVE_TIMEOUT) -> str:
        loop = asyncio.get_event_loop()
        f: asyncio.Future[str] = loop.create_future()
        self._pending[action_id] = f
        try:
            return await asyncio.wait_for(asyncio.shield(f), timeout=timeout)
        except asyncio.TimeoutError:
            _dbg(f"wait_button timeout for {action_id}")
            return "timeout"
        finally:
            self._pending.pop(action_id, None)

    # ── TCP 客户端处理 ────────────────────────────────────────────────────────

    async def _handle_client(self, reader: asyncio.StreamReader,
                             writer: asyncio.StreamWriter):
        try:
            raw = await asyncio.wait_for(reader.read(4096), timeout=5.0)
            req = json.loads(raw.decode())
            cmd = req.get("cmd", "")
            _dbg(f"client cmd: {cmd}")

            if cmd == "ping":
                resp = {"ok": True, "connected": self._connected}

            elif cmd == "set_state":
                ok = await self.send_state(req["state"])
                resp = {"ok": ok}

            elif cmd == "attention":
                tool_name = req.get("tool_name", "")
                action_id = req.get("action_id", str(uuid.uuid4())[:8])

                if tool_name and tool_name in self._auto_approved:
                    # Already approved for this session — skip ATTENTION entirely
                    _dbg(f"auto-approved: {tool_name}")
                    resp = {"decision": "approve"}
                else:
                    await self.send_state(req["state"])
                    decision = await self.wait_button(action_id,
                                                      timeout=req.get("timeout", APPROVE_TIMEOUT))
                    if decision == "navigate" and tool_name:
                        self._auto_approved.add(tool_name)
                        _dbg(f"session auto-approve added: {tool_name}")
                    resp = {"decision": decision}

            elif cmd == "stop_daemon":
                resp = {"ok": True}
                writer.write((json.dumps(resp) + "\n").encode())
                await writer.drain()
                writer.close()
                asyncio.get_event_loop().stop()
                return

            else:
                resp = {"error": f"unknown cmd: {cmd}"}

            writer.write((json.dumps(resp) + "\n").encode())
            await writer.drain()
        except Exception as e:
            try:
                writer.write((json.dumps({"error": str(e)}) + "\n").encode())
                await writer.drain()
            except Exception:
                pass
        finally:
            try:
                writer.close()
            except Exception:
                pass

    # ── 主入口 ────────────────────────────────────────────────────────────────

    async def run(self):
        server = await asyncio.start_server(self._handle_client, DAEMON_HOST, DAEMON_PORT)
        asyncio.create_task(self._ble_loop())
        addr = server.sockets[0].getsockname()
        print(f"buddy_daemon: listening on {addr[0]}:{addr[1]}", flush=True)
        async with server:
            await server.serve_forever()


# ═══════════════════════════════════════════════════════════════════════════════
# CLIENT (hook 脚本侧)
# ═══════════════════════════════════════════════════════════════════════════════

def _daemon_send(req: dict, wait_response: bool = True, timeout: float = 35.0) -> dict | None:
    """向 daemon 发送请求，可选等待响应。"""
    try:
        with socket.create_connection((DAEMON_HOST, DAEMON_PORT), timeout=2.0) as s:
            s.sendall(json.dumps(req).encode())
            if not wait_response:
                return None
            s.shutdown(socket.SHUT_WR)
            chunks = []
            s.settimeout(timeout)
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
            data = b"".join(chunks)
            return json.loads(data.decode()) if data else {}
    except (ConnectionRefusedError, OSError):
        return None


def _ensure_daemon() -> bool:
    """确认 daemon 运行中，否则自动启动。"""
    resp = _daemon_send({"cmd": "ping"}, timeout=1.0)
    if resp and resp.get("ok"):
        return True

    # 启动 daemon
    import subprocess
    kwargs: dict = {
        "stdout": subprocess.DEVNULL,
        "stderr": subprocess.DEVNULL,
    }
    if os.name == "nt":
        kwargs["creationflags"] = 0x00000008 | 0x00000200  # DETACHED_PROCESS | CREATE_NO_WINDOW
    else:
        kwargs["start_new_session"] = True

    _dbg("starting daemon …")
    subprocess.Popen([sys.executable, __file__, "daemon"], **kwargs)

    # 等待 daemon 就绪（最多 12 秒，包含 BLE 扫描时间）
    for _ in range(24):
        time.sleep(0.5)
        resp = _daemon_send({"cmd": "ping"}, timeout=0.5)
        if resp and resp.get("ok"):
            _dbg("daemon ready")
            return True

    print("buddy_hook: daemon 启动超时", file=sys.stderr)
    return False


# ── hook 命令 ─────────────────────────────────────────────────────────────────

def _state_msg(state: str, **kwargs) -> dict:
    return {"type": "state_update", "state": state, **kwargs}



def cmd_busy(_hook_data: dict) -> None:
    if not _ensure_daemon():
        return
    _daemon_send({"cmd": "set_state", "state": _state_msg("BUSY")},
                 wait_response=False)


def cmd_celebrate(_hook_data: dict) -> None:
    if not _ensure_daemon():
        return
    _daemon_send({"cmd": "set_state", "state": _state_msg("CELEBRATE")},
                 wait_response=False)


def cmd_idle(_hook_data: dict) -> None:
    if not _ensure_daemon():
        return
    _daemon_send({"cmd": "set_state", "state": _state_msg("IDLE")},
                 wait_response=False)


def cmd_attention(hook_data: dict) -> None:
    """发送 ATTENTION，保持连接等待按键（Notification hook 不返回 block 决定）。"""
    if not _ensure_daemon():
        return
    msg = hook_data.get("message", "")
    action_id = str(uuid.uuid4())[:8]
    state = _state_msg("ATTENTION", message=msg[:64],
                       action={"id": action_id, "prompt": msg[:100]})
    _daemon_send({"cmd": "attention", "action_id": action_id,
                  "state": state, "timeout": APPROVE_TIMEOUT},
                 wait_response=True, timeout=APPROVE_TIMEOUT + 5)
    # 忽略 decision — Notification hook 不阻断 Claude Code


def cmd_approve(hook_data: dict) -> None:
    """发送 ATTENTION + 等待按键，B 键返回 block 决定阻断工具调用。"""
    if not _ensure_daemon():
        # Daemon unavailable — approve so Claude Code isn't stuck
        print(json.dumps({"decision": "approve"}))
        return
    tool_name  = hook_data.get("tool_name", "")
    tool_input = hook_data.get("tool_input", {})
    if tool_name == "Bash":
        prompt = tool_input.get("command", "")[:80]
    elif tool_name in ("Edit", "MultiEdit"):
        prompt = tool_input.get("file_path", "")[:80]
    elif tool_name == "Write":
        prompt = tool_input.get("file_path", "")[:80]
    else:
        prompt = tool_name

    action_id = str(uuid.uuid4())[:8]
    state = _state_msg("ATTENTION",
                       action={"id": action_id, "prompt": prompt})
    resp = _daemon_send({"cmd": "attention", "action_id": action_id,
                         "tool_name": tool_name,
                         "state": state, "timeout": APPROVE_TIMEOUT},
                        wait_response=True, timeout=APPROVE_TIMEOUT + 5)

    decision = (resp or {}).get("decision", "approve")
    if decision == "deny":
        print(json.dumps({"decision": "block",
                          "reason": "BuddyPixel 设备按键拒绝"}))
    elif decision == "navigate":
        # A 键 = 本次会话内不再询问此工具
        print(json.dumps({"decision": "approve"}))
    else:
        # OK 键 or timeout — approve this call only.
        # Must explicitly approve — silent exit 0 doesn't bypass Claude Code's
        # own interactive permission prompt.
        print(json.dumps({"decision": "approve"}))


def cmd_status(_hook_data: dict) -> None:
    resp = _daemon_send({"cmd": "ping"}, timeout=1.0)
    if resp and resp.get("ok"):
        connected = resp.get("connected", False)
        print(f"daemon: 运行中  BLE: {'已连接' if connected else '未连接'}")
    else:
        print("daemon: 未运行")


def cmd_stop(_hook_data: dict) -> None:
    resp = _daemon_send({"cmd": "stop_daemon"}, timeout=2.0)
    if resp:
        print("daemon: 已停止")
    else:
        print("daemon: 未运行或停止失败")


def cmd_scan(_hook_data: dict) -> None:
    async def _do():
        from bleak import BleakScanner
        print(f"正在扫描 {DEVICE_NAME} …")
        device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=SCAN_TIMEOUT)
        if device:
            ADDR_CACHE.write_text(device.address)
            print(f"找到: {device.name} ({device.address})")
            print(f"地址已缓存到 {ADDR_CACHE}")
        else:
            print(f"未找到 {DEVICE_NAME}")
    asyncio.run(_do())


def cmd_clear(_hook_data: dict) -> None:
    ADDR_CACHE.unlink(missing_ok=True)
    print(f"已清除缓存 ({ADDR_CACHE})")


# ─── 入口 ─────────────────────────────────────────────────────────────────────

_COMMANDS = {
    "busy":      cmd_busy,
    "celebrate": cmd_celebrate,
    "idle":      cmd_idle,
    "attention": cmd_attention,
    "approve":   cmd_approve,
    "status":    cmd_status,
    "stop":      cmd_stop,
    "scan":      cmd_scan,
    "clear":     cmd_clear,
}


def main() -> None:
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""

    if cmd == "daemon":
        bg = "--bg" in sys.argv
        if bg:
            import subprocess
            kwargs: dict = {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
            if os.name == "nt":
                kwargs["creationflags"] = 0x00000008 | 0x00000200
            else:
                kwargs["start_new_session"] = True
            subprocess.Popen(
                [sys.executable, __file__, "daemon"],
                **kwargs
            )
            print("buddy_daemon: 已在后台启动")
            return
        asyncio.run(BuddyDaemon().run())
        return

    hook_data = _read_stdin()
    handler = _COMMANDS.get(cmd)
    if handler:
        handler(hook_data)
    else:
        print(__doc__)


if __name__ == "__main__":
    main()

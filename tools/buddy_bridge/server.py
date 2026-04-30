#!/usr/bin/env python3
"""
buddy_bridge/server.py
======================
Bridge server between Claude Desktop (BLE) and TUYA_T5AI_PIXEL (WebSocket).

HTTP API (for web dashboard):
  GET  /            → static index.html

WebSocket API:
  WS   /device      → device connection (plain TCP, no TLS)
  WS   /ws          → dashboard live updates

Usage:
  pip install -r requirements.txt
  python server.py [--host 0.0.0.0] [--port 8765] [--no-ble]
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import subprocess
import time
import uuid
import logging
from dataclasses import dataclass, asdict, field
from typing import Optional, Set
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Request
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

# Optional BLE (bleak) — graceful degradation if not installed
try:
    from bleak import BleakClient, BleakScanner
    BLE_AVAILABLE = True
except ImportError:
    BLE_AVAILABLE = False

# ─── Logging ────────────────────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("buddy_bridge")

# ─── BLE Nordic UART UUIDs (Claude Desktop Hardware Buddy) ──────────────────
NUS_SERVICE  = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX       = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # write → desktop
NUS_TX       = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # notify → us

# ─── BLE Tuya Pixel UUIDs (buddy_pixel device) ──────────────────────────────
PIXEL_DEVICE_NAME = "BuddyPixel"
PIXEL_WRITE_CHAR  = "00000001-0000-1001-8001-00805f9b07d0"  # bridge→pixel: state_update
PIXEL_NOTIFY_CHAR = "00000002-0000-1001-8001-00805f9b07d0"  # pixel→bridge: events

# ─── State dataclasses ──────────────────────────────────────────────────────
VALID_STATES = {"DISCONNECTED", "IDLE", "BUSY", "ATTENTION", "CELEBRATE", "DIZZY"}

@dataclass
class PendingAction:
    id: str = ""
    prompt: str = ""

@dataclass
class BuddyState:
    state: str = "DISCONNECTED"
    sessions: int = 0
    tokens: int = 0
    message: str = ""
    action: PendingAction = field(default_factory=PendingAction)

    def to_api_dict(self) -> dict:
        return {
            "state":    self.state,
            "sessions": self.sessions,
            "tokens":   self.tokens,
            "message":  self.message,
            "action": {
                "id":     self.action.id,
                "prompt": self.action.prompt,
            } if self.action.id else {},
        }

# ─── Global mutable state ───────────────────────────────────────────────────
class BridgeServer:
    def __init__(self):
        self.buddy_state    = BuddyState()
        self.ble_connected  = False
        self.ble_device_name = ""
        self.ws_clients: Set[WebSocket] = set()
        self.device_ws: Optional[WebSocket] = None   # device WebSocket connection
        self.event_log: list[dict] = []             # last 100 events
        self._ble_client: Optional[BleakClient] = None
        self._ble_rx_buf: str = ""
        self._ble_task: Optional[asyncio.Task] = None
        # Pixel BLE (buddy_pixel device)
        self.pixel_ble_connected: bool = False
        self.pixel_ble_device_name: str = ""
        self._pixel_ble_client: Optional[BleakClient] = None
        self._pixel_ble_rx_buf: str = ""
        self._pixel_ble_task: Optional[asyncio.Task] = None

    # ── log helpers ──────────────────────────────────────────────────────────
    def _add_log(self, source: str, direction: str, text: str):
        entry = {
            "ts":        time.strftime("%H:%M:%S"),
            "source":    source,
            "direction": direction,
            "text":      text[:200],
        }
        self.event_log.append(entry)
        if len(self.event_log) > 100:
            self.event_log.pop(0)
        return entry

    async def broadcast(self, msg: dict):
        dead = set()
        for ws in self.ws_clients:
            try:
                await ws.send_text(json.dumps(msg))
            except Exception:
                dead.add(ws)
        self.ws_clients -= dead

    async def push_state(self):
        msg = {
            "type":                "state_update",
            "buddy":               self.buddy_state.to_api_dict(),
            "ble_connected":       self.ble_connected,
            "ble_device":          self.ble_device_name,
            "pixel_ble_connected": self.pixel_ble_connected,
            "pixel_ble_device":    self.pixel_ble_device_name,
            "device_online":       self.device_ws is not None,
        }
        await self.broadcast(msg)
        # Push state to device WebSocket (flat format the C client expects)
        await self.push_device_state()

    async def push_device_state(self):
        """Push current state to the device WebSocket connection and pixel BLE."""
        msg = {"type": "state_update", **self.buddy_state.to_api_dict()}
        if self.device_ws is not None:
            try:
                await self.device_ws.send_text(json.dumps(msg))
            except Exception:
                self.device_ws = None
        await self.pixel_ble_send(msg)

    async def push_log(self, source: str, direction: str, text: str):
        entry = self._add_log(source, direction, text)
        await self.broadcast({"type": "log", "entry": entry})

    # ── state mutation ───────────────────────────────────────────────────────
    async def set_state(self, new_state: str, **kwargs):
        old = self.buddy_state.state
        if new_state in VALID_STATES:
            self.buddy_state.state = new_state
        for k, v in kwargs.items():
            if hasattr(self.buddy_state, k):
                setattr(self.buddy_state, k, v)
        if old != self.buddy_state.state:
            log.info("State %s → %s", old, self.buddy_state.state)
            await self.push_log("bridge", "→", f"State: {old} → {new_state}")
        await self.push_state()

    # ── BLE: receive handler (Claude Desktop → bridge) ───────────────────────
    def _ble_rx(self, _sender, data: bytearray):
        self._ble_rx_buf += data.decode("utf-8", errors="replace")
        while "\n" in self._ble_rx_buf:
            line, self._ble_rx_buf = self._ble_rx_buf.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            asyncio.create_task(self._handle_ble_msg(line))

    async def _handle_ble_msg(self, line: str):
        await self.push_log("claude_desktop", "←", line)
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            log.warning("BLE non-JSON: %s", line)
            return

        mtype = msg.get("type", "")

        if mtype == "session_state":
            running  = msg.get("running", 0)
            waiting  = msg.get("waiting", 0)
            sessions = msg.get("sessions", 0)
            tokens   = msg.get("tokens", 0)

            if waiting > 0:
                new_state = "ATTENTION"
            elif running > 0:
                new_state = "BUSY"
            elif sessions > 0:
                new_state = "CELEBRATE"
            else:
                new_state = "IDLE"

            await self.set_state(new_state,
                                 sessions=sessions,
                                 tokens=tokens,
                                 message=msg.get("message", ""))

        elif mtype == "permission_request":
            action = PendingAction(
                id=msg.get("id", str(uuid.uuid4())[:8]),
                prompt=msg.get("prompt", "")[:63],
            )
            self.buddy_state.action = action
            await self.set_state("ATTENTION")
            log.info("Permission request: %s", action.prompt)

        elif mtype == "heartbeat":
            if self.buddy_state.state == "DISCONNECTED":
                await self.set_state("IDLE")

        else:
            log.debug("Unknown BLE msg type: %s", mtype)

    # ── BLE: send to Claude Desktop ──────────────────────────────────────────
    async def ble_send(self, obj: dict):
        if not self.ble_connected or self._ble_client is None:
            return
        payload = (json.dumps(obj) + "\n").encode()
        try:
            await self._ble_client.write_gatt_char(NUS_RX, payload)
            await self.push_log("bridge", "→ BLE", json.dumps(obj))
        except Exception as e:
            log.error("BLE send error: %s", e)

    # ── BLE: scanner + connector loop ────────────────────────────────────────
    async def ble_loop(self):
        log.info("BLE loop started (scanning for Claude Desktop Hardware Buddy)")
        while True:
            try:
                await self._ble_scan_and_connect()
            except Exception as e:
                log.warning("BLE error: %s — retrying in 10 s", e)
            await asyncio.sleep(10)

    async def _ble_scan_and_connect(self):
        log.info("Scanning for BLE devices with NUS service …")
        await self.push_log("bridge", "→", "BLE scan started")

        devices = await BleakScanner.discover(timeout=8.0,
                                              service_uuids=[NUS_SERVICE])
        if not devices:
            log.info("No Claude Desktop buddy found in scan")
            return

        device = devices[0]
        log.info("Found: %s (%s)", device.name, device.address)
        await self.push_log("bridge", "←", f"Found BLE: {device.name} {device.address}")

        async with BleakClient(device.address,
                               disconnected_callback=self._ble_disconnected) as client:
            self._ble_client   = client
            self.ble_connected = True
            self.ble_device_name = device.name or device.address
            await self.push_log("bridge", "←", f"BLE connected: {self.ble_device_name}")
            await self.set_state("IDLE")

            await client.start_notify(NUS_TX, self._ble_rx)
            log.info("BLE connected and listening")

            # Keep alive until disconnect
            while client.is_connected:
                await asyncio.sleep(1.0)

    def _ble_disconnected(self, client):
        log.warning("BLE disconnected")
        self.ble_connected  = False
        self._ble_client    = None
        asyncio.create_task(self.set_state("DISCONNECTED"))
        asyncio.create_task(self.push_log("bridge", "→", "BLE disconnected"))

    # ── Pixel BLE: send state update to buddy_pixel ──────────────────────────
    async def pixel_ble_send(self, obj: dict):
        if not self.pixel_ble_connected or self._pixel_ble_client is None:
            return
        payload = (json.dumps(obj) + "\n").encode()
        try:
            await self._pixel_ble_client.write_gatt_char(
                PIXEL_WRITE_CHAR, payload, response=False
            )
        except Exception as e:
            log.error("Pixel BLE send error: %s", e)

    # ── Pixel BLE: receive events from buddy_pixel ───────────────────────────
    def _pixel_ble_rx(self, _sender, data: bytearray):
        self._pixel_ble_rx_buf += data.decode("utf-8", errors="replace")
        while "\n" in self._pixel_ble_rx_buf:
            line, self._pixel_ble_rx_buf = self._pixel_ble_rx_buf.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            asyncio.create_task(self._handle_pixel_event(line))

    async def _handle_pixel_event(self, line: str):
        await self.push_log("pixel_ble", "←", line)
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            log.warning("Pixel BLE non-JSON: %s", line)
            return
        if msg.get("type") != "event":
            return
        etype     = msg.get("event", "")
        action_id = msg.get("action_id", "")
        log.info("Pixel BLE event: %s id=%s", etype, action_id)
        if etype in ("approve", "deny"):
            await self.ble_send({"type": etype, "id": action_id})
            self.buddy_state.action = PendingAction()
            await self.set_state("BUSY")
        elif etype in ("shake", "navigate"):
            await self.push_log("bridge", "→", f"Pixel {etype}")

    # ── Pixel BLE: scanner + connector loop ──────────────────────────────────
    async def pixel_ble_loop(self):
        log.info("Pixel BLE loop started (scanning for %s)", PIXEL_DEVICE_NAME)
        while True:
            try:
                await self._pixel_ble_scan_and_connect()
            except Exception as e:
                log.warning("Pixel BLE error: %s — retrying in 5 s", e)
            await asyncio.sleep(5)

    async def _pixel_ble_scan_and_connect(self):
        log.info("Scanning for %s …", PIXEL_DEVICE_NAME)
        device = await BleakScanner.find_device_by_name(
            PIXEL_DEVICE_NAME, timeout=10.0
        )
        if device is None:
            log.info("%s not found in scan", PIXEL_DEVICE_NAME)
            return
        log.info("Found pixel: %s (%s)", device.name, device.address)
        await self.push_log("bridge", "←",
                            f"Found pixel BLE: {device.name} {device.address}")

        async with BleakClient(
            device.address,
            disconnected_callback=self._pixel_ble_disconnected,
        ) as client:
            self._pixel_ble_client     = client
            self.pixel_ble_connected   = True
            self.pixel_ble_device_name = device.name or device.address
            await self.push_log("bridge", "←",
                                f"Pixel BLE connected: {self.pixel_ble_device_name}")

            # NimBLE may emit a services_changed event right after connection,
            # invalidating the GATT cache.  Wait briefly then retry start_notify.
            await asyncio.sleep(0.5)
            for attempt in range(3):
                try:
                    await client.start_notify(PIXEL_NOTIFY_CHAR, self._pixel_ble_rx)
                    break
                except Exception as e:
                    if attempt < 2:
                        log.warning("start_notify attempt %d/3 failed: %s — retrying",
                                    attempt + 1, e)
                        await asyncio.sleep(1.0)
                    else:
                        raise
            log.info("Pixel BLE connected and listening")

            # Push current state immediately so the pixel renders the right animation
            await self.pixel_ble_send(
                {"type": "state_update", **self.buddy_state.to_api_dict()}
            )
            await self.push_state()   # update dashboard pixel-online indicator

            while client.is_connected:
                await asyncio.sleep(1.0)

    def _pixel_ble_disconnected(self, client):
        log.warning("Pixel BLE disconnected")
        self.pixel_ble_connected  = False
        self._pixel_ble_client    = None
        asyncio.create_task(self.push_log("bridge", "→", "Pixel BLE disconnected"))
        asyncio.create_task(self.push_state())


# ─── Application singleton ──────────────────────────────────────────────────
bridge = BridgeServer()


# ─── FastAPI app ─────────────────────────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    if BLE_AVAILABLE:
        bridge._ble_task = asyncio.create_task(bridge.ble_loop())
        bridge._pixel_ble_task = asyncio.create_task(bridge.pixel_ble_loop())
        log.info("BLE background tasks started")
    else:
        log.warning("bleak not installed — BLE disabled. Install with: pip install bleak")
    yield
    if bridge._ble_task:
        bridge._ble_task.cancel()
    if bridge._pixel_ble_task:
        bridge._pixel_ble_task.cancel()


app = FastAPI(title="Buddy Bridge", lifespan=lifespan)


# ─── Device WebSocket API ────────────────────────────────────────────────────

@app.websocket("/device")
async def device_ws_endpoint(ws: WebSocket):
    """Device connects here; bridge pushes state updates and receives events."""
    await ws.accept()
    bridge.device_ws = ws
    log.info("Device WebSocket connected")
    await bridge.push_log("device", "←", "WS connected")
    await bridge.push_state()   # update dashboard device-online indicator

    # Send current state immediately so device renders the right animation
    await bridge.push_device_state()

    try:
        while True:
            try:
                # 20-second timeout prevents WinError 121 on idle connections
                text = await asyncio.wait_for(ws.receive_text(), timeout=20.0)
            except asyncio.TimeoutError:
                # Send a keepalive ping; device ignores unknown type, TCP stays alive
                try:
                    await ws.send_text(json.dumps({"type": "ping"}))
                except Exception:
                    break
                continue

            try:
                body = json.loads(text)
            except Exception:
                continue

            if body.get("type") in ("ping", "pong"):
                continue   # device keepalive echo — ignore

            if body.get("type") != "event":
                continue

            etype     = body.get("event", "")
            action_id = body.get("action_id", "")
            log.info("Device WS event: type=%s action_id=%s", etype, action_id)
            await bridge.push_log("device", "←", f"ws event:{etype} id:{action_id}")

            if etype in ("approve", "deny"):
                await bridge.ble_send({"type": etype, "id": action_id})
                bridge.buddy_state.action = PendingAction()
                await bridge.set_state("BUSY")
            elif etype == "shake":
                await bridge.push_log("bridge", "→", "Shake detected on device")
            elif etype == "navigate":
                await bridge.push_log("bridge", "→", "Navigate button pressed")

    except WebSocketDisconnect:
        pass
    except Exception as e:
        log.debug("Device WS error: %s", e)
    finally:
        bridge.device_ws = None
        log.info("Device WebSocket disconnected")
        await bridge.push_log("device", "→", "WS disconnected")
        await bridge.push_state()   # update dashboard device-online indicator


# ─── Manual control API (for testing without Claude Desktop) ─────────────────

@app.post("/api/test/set_state")
async def test_set_state(request: Request):
    """Debug endpoint: manually set buddy state from the dashboard."""
    body = await request.json()
    new_state = body.get("state", "IDLE")
    prompt    = body.get("prompt", "")
    sessions  = body.get("sessions", bridge.buddy_state.sessions)
    tokens    = body.get("tokens", bridge.buddy_state.tokens)
    message   = body.get("message", "")

    action = PendingAction()
    if new_state == "ATTENTION" and prompt:
        action = PendingAction(id=str(uuid.uuid4())[:8], prompt=prompt[:63])

    bridge.buddy_state.action   = action
    bridge.buddy_state.sessions = sessions
    bridge.buddy_state.tokens   = tokens
    bridge.buddy_state.message  = message
    await bridge.set_state(new_state)
    return {"ok": True, "state": bridge.buddy_state.to_api_dict()}


@app.post("/api/test/simulate_ble")
async def test_simulate_ble(request: Request):
    """Simulate a BLE message from Claude Desktop (for testing)."""
    body = await request.json()
    line = json.dumps(body)
    await bridge._handle_ble_msg(line)
    return {"ok": True}


@app.post("/api/test/send_event")
async def test_send_event(request: Request):
    """Simulate a device button event (approve / deny / shake / navigate)."""
    body      = await request.json()
    etype     = body.get("event", "")
    action_id = body.get("action_id", "")

    await bridge.push_log("dashboard", "→", f"event:{etype} id:{action_id}")

    if etype in ("approve", "deny"):
        await bridge.ble_send({"type": etype, "id": action_id})
        bridge.buddy_state.action = PendingAction()
        await bridge.set_state("BUSY")
    elif etype == "shake":
        await bridge.push_log("bridge", "→", "Shake detected (dashboard)")
    elif etype == "navigate":
        await bridge.push_log("bridge", "→", "Navigate (dashboard)")

    return {"ok": True}


# ─── WebSocket (dashboard live updates) ──────────────────────────────────────

@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    bridge.ws_clients.add(ws)

    # Send current state on connect
    await ws.send_text(json.dumps({
        "type":                "init",
        "buddy":               bridge.buddy_state.to_api_dict(),
        "ble_connected":       bridge.ble_connected,
        "ble_device":          bridge.ble_device_name,
        "pixel_ble_connected": bridge.pixel_ble_connected,
        "pixel_ble_device":    bridge.pixel_ble_device_name,
        "device_online":       bridge.device_ws is not None,
        "log":                 bridge.event_log[-50:],
    }))

    try:
        while True:
            await ws.receive_text()   # keep alive (dashboard sends pings)
    except WebSocketDisconnect:
        bridge.ws_clients.discard(ws)


# ─── Serve dashboard ─────────────────────────────────────────────────────────

@app.get("/", response_class=HTMLResponse)
async def dashboard():
    import os
    html_path = os.path.join(os.path.dirname(__file__), "static", "index.html")
    with open(html_path, "r", encoding="utf-8") as f:
        return f.read()


# ─── CLI entry ───────────────────────────────────────────────────────────────

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CERT_FILE  = os.path.join(_SCRIPT_DIR, "cert.pem")
_KEY_FILE   = os.path.join(_SCRIPT_DIR, "key.pem")


def _ensure_cert():
    """Auto-generate a self-signed TLS certificate if not present."""
    if os.path.exists(_CERT_FILE) and os.path.exists(_KEY_FILE):
        return
    log.info("正在生成自签名 TLS 证书 (cert.pem / key.pem) ...")
    try:
        subprocess.run(
            [
                "openssl", "req", "-x509",
                "-newkey", "rsa:2048",
                "-keyout", _KEY_FILE,
                "-out",    _CERT_FILE,
                "-days",   "3650",
                "-nodes",
                "-subj",   "/CN=buddy-bridge",
            ],
            check=True,
            capture_output=True,
        )
        log.info("证书已生成: %s", _CERT_FILE)
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        log.error("openssl 不可用，无法生成证书: %s", e)
        raise SystemExit(1)


def main():
    parser = argparse.ArgumentParser(description="Buddy Bridge Server")
    parser.add_argument("--host",   default="0.0.0.0",  help="绑定地址")
    parser.add_argument("--port",   type=int, default=8765, help="监听端口")
    parser.add_argument("--no-ble", action="store_true",   help="禁用 BLE")
    args = parser.parse_args()

    if args.no_ble:
        global BLE_AVAILABLE
        BLE_AVAILABLE = False
        log.info("BLE 已通过 --no-ble 禁用")

    log.info("Buddy Bridge 已启动 (plain HTTP): http://%s:%d", args.host, args.port)
    log.info("控制台地址: http://localhost:%d", args.port)
    uvicorn.run(
        app,
        host=args.host,
        port=args.port,
        log_level="warning",
    )


if __name__ == "__main__":
    main()

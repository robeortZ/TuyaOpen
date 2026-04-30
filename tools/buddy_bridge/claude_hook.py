#!/usr/bin/env python3
"""
claude_hook.py — Claude Code hook for Buddy Pixel status.

Reads hook event data from stdin (JSON), maps it to a Buddy state,
and POSTs to the local buddy_bridge server so the pixel display
updates automatically while Claude Code is working.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  Setup: add to ~/.claude/settings.json
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": ".*",
        "hooks": [{"type": "command",
                   "command": "python3 /path/to/claude_hook.py pre_tool"}]
      }
    ],
    "PostToolUse": [
      {
        "matcher": ".*",
        "hooks": [{"type": "command",
                   "command": "python3 /path/to/claude_hook.py post_tool"}]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [{"type": "command",
                   "command": "python3 /path/to/claude_hook.py stop"}]
      }
    ],
    "Notification": [
      {
        "matcher": "",
        "hooks": [{"type": "command",
                   "command": "python3 /path/to/claude_hook.py notification"}]
      }
    ]
  }
}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  Environment variables
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  BUDDY_BRIDGE_URL   full URL of the bridge server
                     (default: http://localhost:8765)

  BUDDY_HOOK_DEBUG   set to 1 to print debug info to stderr
"""

from __future__ import annotations

import json
import os
import sys
import urllib.request
import urllib.error
import tempfile
import time

# ─── Config ──────────────────────────────────────────────────────────────────
BRIDGE_URL  = os.environ.get("BUDDY_BRIDGE_URL", "http://localhost:8765")
DEBUG       = os.environ.get("BUDDY_HOOK_DEBUG", "0") == "1"

# Per-session tracking file  (keyed by session_id to handle parallel sessions)
_STATE_DIR  = os.path.join(tempfile.gettempdir(), "buddy_hook")


def _dbg(msg: str) -> None:
    if DEBUG:
        print(f"[buddy_hook] {msg}", file=sys.stderr)


# ─── Session state helpers ───────────────────────────────────────────────────

def _state_path(session_id: str) -> str:
    os.makedirs(_STATE_DIR, exist_ok=True)
    safe = session_id.replace("/", "_").replace("\\", "_")[:32]
    return os.path.join(_STATE_DIR, f"{safe}.json")


def _load_state(session_id: str) -> dict:
    try:
        with open(_state_path(session_id)) as f:
            return json.load(f)
    except Exception:
        return {"sessions": 0, "tokens": 0, "tool_count": 0}


def _save_state(session_id: str, state: dict) -> None:
    try:
        with open(_state_path(session_id), "w") as f:
            json.dump(state, f)
    except Exception:
        pass


def _global_sessions() -> int:
    """Count total completed sessions across all tracked session files."""
    total = 0
    try:
        for fname in os.listdir(_STATE_DIR):
            if fname.endswith(".json"):
                try:
                    with open(os.path.join(_STATE_DIR, fname)) as f:
                        d = json.load(f)
                        total += d.get("sessions", 0)
                except Exception:
                    pass
    except Exception:
        pass
    return total


# ─── Bridge communication ─────────────────────────────────────────────────────

def _post(endpoint: str, data: dict) -> None:
    url = BRIDGE_URL.rstrip("/") + endpoint
    try:
        payload = json.dumps(data).encode()
        req = urllib.request.Request(
            url, data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=1):
            pass
        _dbg(f"POST {endpoint} → {data}")
    except urllib.error.URLError as e:
        _dbg(f"POST {endpoint} failed (bridge offline?): {e}")
    except Exception as e:
        _dbg(f"POST {endpoint} error: {e}")


def _simulate_ble(msg_type: str, **kwargs) -> None:
    _post("/api/test/simulate_ble", {"type": msg_type, **kwargs})


# ─── Hook handlers ────────────────────────────────────────────────────────────

def handle_pre_tool(hook_data: dict) -> None:
    """Claude is about to call a tool → BUSY."""
    session_id = hook_data.get("session_id", "default")
    tool_name  = hook_data.get("tool_name", "tool")
    state      = _load_state(session_id)
    state["tool_count"] = state.get("tool_count", 0) + 1
    _save_state(session_id, state)

    _simulate_ble(
        "session_state",
        running=1,
        waiting=0,
        sessions=_global_sessions(),
        tokens=state.get("tokens", 0),
        message=f"{tool_name}",
    )


def handle_post_tool(hook_data: dict) -> None:
    """Tool just finished — stay BUSY (Claude usually chains tool calls)."""
    # We intentionally do nothing here so the BUSY animation keeps playing
    # until the Stop hook fires.
    pass


def handle_stop(hook_data: dict) -> None:
    """Claude finished responding → CELEBRATE then back to IDLE."""
    session_id = hook_data.get("session_id", "default")
    state      = _load_state(session_id)

    # Increment completed-session counter
    state["sessions"] = state.get("sessions", 0) + 1
    _save_state(session_id, state)

    total_sessions = _global_sessions()

    _simulate_ble(
        "session_state",
        running=0,
        waiting=0,
        sessions=total_sessions,
        tokens=state.get("tokens", 0),
        message="",
    )


def handle_notification(hook_data: dict) -> None:
    """A notification was shown — may be a permission/approval request."""
    message = hook_data.get("message", "")
    _dbg(f"notification: {message!r}")

    # Claude Code permission requests ask the user to allow a tool action.
    # The message text typically contains these keywords:
    permission_keywords = [
        "allow", "approve", "permission", "bash", "execute", "run",
        "write", "delete", "read", "create", "rm ", "mv ", "cp ",
        "允许", "批准", "权限",
    ]
    is_permission = any(k.lower() in message.lower()
                        for k in permission_keywords)

    if is_permission:
        _simulate_ble(
            "permission_request",
            id="hook-" + str(int(time.time()))[-6:],
            prompt=message[:63],
        )
    else:
        session_id = hook_data.get("session_id", "default")
        state      = _load_state(session_id)
        _simulate_ble(
            "session_state",
            running=0,
            waiting=1,
            sessions=_global_sessions(),
            tokens=state.get("tokens", 0),
            message=message[:100],
        )


# ─── Entry point ─────────────────────────────────────────────────────────────

def main() -> None:
    hook_type = sys.argv[1] if len(sys.argv) > 1 else ""

    # Read hook data from stdin
    try:
        raw = sys.stdin.read()
        hook_data = json.loads(raw) if raw.strip() else {}
    except Exception:
        hook_data = {}

    _dbg(f"hook_type={hook_type!r} data={hook_data}")

    dispatch = {
        "pre_tool":    handle_pre_tool,
        "post_tool":   handle_post_tool,
        "stop":        handle_stop,
        "notification": handle_notification,
    }

    handler = dispatch.get(hook_type)
    if handler:
        handler(hook_data)
    else:
        _dbg(f"unknown hook type: {hook_type!r}")


if __name__ == "__main__":
    main()

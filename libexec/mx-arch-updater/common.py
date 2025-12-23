#!/usr/bin/env python3

import hashlib
import json
import os
from pathlib import Path
from typing import Any, Dict

from PySide6 import QtCore

APP_ORG = "MX-Linux"
APP_NAME = "mx-arch-updater"

ENV_ROOT = "MX_ARCH_UPDATER_PATH"

STATE_DIR = Path("/var/lib/mx-arch-updater")
STATE_FILE = STATE_DIR / "state.json"
DEFAULT_DATA_ROOT = Path("/usr/share/mx-arch-updater")
DEFAULT_HELPER_ROOT = Path("/usr/lib/mx-arch-updater")

DEFAULT_CHECK_INTERVAL = 60 * 30
DEFAULT_IDLE_TIMEOUT = 4 * 60

ICON_THEMES = ["wireframe-dark", "wireframe-light", "classic", "pulse"]
UPGRADE_MODES = ["basic", "full"]


def ensure_not_root() -> None:
    if os.geteuid() == 0:
        raise SystemExit("This application must run in a user session, not as root.")


def settings() -> QtCore.QSettings:
    return QtCore.QSettings(APP_ORG, APP_NAME)


def default_state() -> Dict[str, Any]:
    return {
        "checked_at": 0,
        "counts": {"upgrade": 0, "new": 0, "remove": 0, "held": 0},
        "packages": [],
        "errors": [],
        "status": "idle",
    }


def _state_checksum(state: Dict[str, Any]) -> str:
    payload = json.dumps(state, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def write_state(state: Dict[str, Any], path: Path = STATE_FILE) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "state": state,
        "checksum": _state_checksum(state),
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def read_state(path: Path = STATE_FILE, require_checksum: bool = True) -> Dict[str, Any]:
    if not path.exists():
        return default_state()
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return default_state()
    state = payload.get("state")
    checksum = payload.get("checksum")
    if not isinstance(state, dict):
        return default_state()
    if require_checksum and checksum != _state_checksum(state):
        return default_state()
    return state


def env_root() -> Path:
    root = os.environ.get(ENV_ROOT)
    if root:
        return Path(root)
    if DEFAULT_DATA_ROOT.exists():
        return DEFAULT_DATA_ROOT
    raise SystemExit(f"{ENV_ROOT} is not set; run via bin launchers.")


def icon_path(theme: str, name: str) -> Path:
    root = env_root()
    return root / "icons" / theme / name


def helper_path(name: str) -> Path:
    root = env_root()
    candidate = root / "lib" / "mx-arch-updater" / name
    if candidate.exists():
        return candidate
    return DEFAULT_HELPER_ROOT / name


def read_setting(key: str, default: Any) -> Any:
    return settings().value(key, default)


def write_setting(key: str, value: Any) -> None:
    s = settings()
    s.setValue(key, value)
    s.sync()

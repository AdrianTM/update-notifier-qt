#!/usr/bin/env python3

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from typing import Dict, List

from PySide6 import QtCore, QtDBus

from common import (
    DEFAULT_CHECK_INTERVAL,
    DEFAULT_IDLE_TIMEOUT,
    STATE_FILE,
    default_state,
    read_state,
    settings,
    write_state,
)

SYSTEM_SERVICE_NAME = "org.mxlinux.UpdaterSystemMonitor"
SYSTEM_OBJECT_PATH = "/org/mxlinux/UpdaterSystemMonitor"
SYSTEM_INTERFACE = "org.mxlinux.UpdaterSystemMonitor"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="MX Arch Updater system monitor")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--no-checksum", action="store_true")
    return parser.parse_args()


_UPDATE_RE = re.compile(r"^(?P<name>\\S+)\\s+(?P<old>\\S+)\\s+->\\s+(?P<new>\\S+)")


def run_pacman_query() -> List[str]:
    if not shutil.which("pacman"):
        return []
    try:
        result = subprocess.run(
            ["pacman", "-Qu"],
            check=False,
            text=True,
            capture_output=True,
        )
    except OSError:
        return []
    if result.returncode not in (0, 1):
        return []
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    return lines


def parse_update_lines(lines: List[str]) -> List[Dict[str, str]]:
    updates: List[Dict[str, str]] = []
    for line in lines:
        match = _UPDATE_RE.match(line)
        if match:
            updates.append(
                {
                    "name": match.group("name"),
                    "old": match.group("old"),
                    "new": match.group("new"),
                    "raw": line,
                }
            )
        else:
            name = line.split()[0]
            updates.append({"name": name, "old": "", "new": "", "raw": line})
    return updates


def parse_pacman_conf(path: str = "/etc/pacman.conf") -> Dict[str, List[str]]:
    ignore_pkg: List[str] = []
    ignore_group: List[str] = []
    if not os.path.exists(path):
        return {"ignore_pkg": ignore_pkg, "ignore_group": ignore_group}
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.split("#", 1)[0].strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("IgnorePkg"):
                _, value = line.split("=", 1)
                ignore_pkg.extend(value.split())
            if line.startswith("IgnoreGroup"):
                _, value = line.split("=", 1)
                ignore_group.extend(value.split())
    return {"ignore_pkg": ignore_pkg, "ignore_group": ignore_group}


def _pacman_field_output(args: List[str], field: str) -> str:
    try:
        result = subprocess.run(args, check=False, text=True, capture_output=True)
    except OSError:
        return ""
    if result.returncode != 0:
        return ""
    for line in result.stdout.splitlines():
        if line.strip().startswith(f"{field}"):
            _, value = line.split(":", 1)
            return value.strip()
    return ""


def get_local_version(pkg: str) -> str:
    return _pacman_field_output(["pacman", "-Qi", pkg], "Version")


def get_sync_version(pkg: str) -> str:
    return _pacman_field_output(["pacman", "-Si", pkg], "Version")


def is_update_available(pkg: str) -> bool:
    if not shutil.which("vercmp"):
        return False
    local_ver = get_local_version(pkg)
    sync_ver = get_sync_version(pkg)
    if not local_ver or not sync_ver:
        return False
    result = subprocess.run(["vercmp", local_ver, sync_ver], capture_output=True, text=True)
    return result.returncode == 0 and result.stdout.strip() == "-1"


def get_replaced_packages(pkg: str) -> List[str]:
    replaces = _pacman_field_output(["pacman", "-Si", pkg], "Replaces")
    if not replaces or replaces.lower() == "none":
        return []
    items = [item.strip(",") for item in replaces.split() if item.strip(",")]
    return items


def get_group_packages(group: str) -> List[str]:
    try:
        result = subprocess.run(
            ["pacman", "-Sqg", group],
            check=False,
            text=True,
            capture_output=True,
        )
    except OSError:
        return []
    if result.returncode != 0:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def build_state(lines: List[str]) -> Dict:
    updates = parse_update_lines(lines)
    now = int(time.time())
    state = default_state()
    state["checked_at"] = now
    state["packages"] = lines
    state["counts"]["upgrade"] = len(updates)
    ignore = parse_pacman_conf()
    held_pkgs = set()
    for pkg in ignore["ignore_pkg"]:
        if is_update_available(pkg):
            held_pkgs.add(pkg)
    for group in ignore["ignore_group"]:
        for pkg in get_group_packages(group):
            if is_update_available(pkg):
                held_pkgs.add(pkg)
    to_remove = set()
    for update in updates:
        for replaced in get_replaced_packages(update["name"]):
            to_remove.add(replaced)
    state["counts"]["remove"] = len(to_remove)
    state["counts"]["held"] = len(held_pkgs)
    state["status"] = "ok"
    return state


class SystemMonitor(QtCore.QObject):
    stateChanged = QtCore.Signal(str)

    def __init__(self, require_checksum: bool):
        super().__init__()
        self.require_checksum = require_checksum
        self._state = read_state(require_checksum=require_checksum)
        self._last_activity = time.time()
        self._check_interval = int(settings().value("Settings/check_interval", DEFAULT_CHECK_INTERVAL))
        self._idle_timeout = int(settings().value("Settings/idle_timeout", DEFAULT_IDLE_TIMEOUT))
        self._check_timer = QtCore.QTimer(self)
        self._check_timer.timeout.connect(self.refresh)
        self._check_timer.start(self._check_interval * 1000)
        self._idle_timer = QtCore.QTimer(self)
        self._idle_timer.timeout.connect(self._check_idle)
        self._idle_timer.start(30 * 1000)

    @QtCore.Slot(result=str)
    def GetState(self) -> str:
        self._touch()
        return json.dumps(self._state)

    @QtCore.Slot()
    def Refresh(self) -> None:
        self.refresh()

    @QtCore.Slot(int)
    def SetIdleTimeout(self, seconds: int) -> None:
        self._idle_timeout = max(30, seconds)
        self._touch()

    def _touch(self) -> None:
        self._last_activity = time.time()

    def refresh(self) -> None:
        self._touch()
        lines = run_pacman_query()
        self._state = build_state(lines)
        write_state(self._state, STATE_FILE)
        self.stateChanged.emit(json.dumps(self._state))

    def _check_idle(self) -> None:
        if time.time() - self._last_activity > self._idle_timeout:
            QtCore.QCoreApplication.quit()


def main() -> int:
    args = parse_args()
    if os.geteuid() != 0:
        sys.stderr.write("ERROR: updater-system-monitor must run as root.\n")
        return 1

    app = QtCore.QCoreApplication(sys.argv)
    bus = QtDBus.QDBusConnection.systemBus()
    if not bus.isConnected():
        sys.stderr.write("ERROR: Could not connect to the system bus.\n")
        return 1
    if not bus.registerService(SYSTEM_SERVICE_NAME):
        sys.stderr.write("ERROR: Could not register system monitor service.\n")
        return 1

    monitor = SystemMonitor(require_checksum=not args.no_checksum)
    bus.registerObject(
        SYSTEM_OBJECT_PATH,
        SYSTEM_INTERFACE,
        monitor,
        QtDBus.QDBusConnection.ExportAllSlots | QtDBus.QDBusConnection.ExportAllSignals,
    )

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

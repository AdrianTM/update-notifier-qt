#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from PySide6 import QtCore, QtGui, QtWidgets, QtDBus

from common import ICON_THEMES, env_root, icon_path

# Constants shared with system monitor
SYSTEM_SERVICE_NAME = "org.mxlinux.UpdaterSystemMonitor"
SYSTEM_OBJECT_PATH = "/org/mxlinux/UpdaterSystemMonitor"
SYSTEM_INTERFACE = "org.mxlinux.UpdaterSystemMonitor"

TRAY_SERVICE_NAME = "org.mxlinux.UpdaterSystemTrayIcon"
TRAY_OBJECT_PATH = "/org/mxlinux/UpdaterSystemTrayIcon"
TRAY_INTERFACE = "org.mxlinux.UpdaterSystemTrayIcon"


class TrayService(QtCore.QObject):

    def __init__(self, tray_app: "TrayApp") -> None:
        super().__init__()
        self.tray_app = tray_app

    @QtCore.Slot()
    def ShowView(self) -> None:
        self.tray_app.open_view()

    @QtCore.Slot()
    def ShowSettings(self) -> None:
        self.tray_app.open_settings()

    @QtCore.Slot()
    def Refresh(self) -> None:
        self.tray_app.refresh()

    @QtCore.Slot()
    def Quit(self) -> None:
        self.tray_app.app.quit()


class TrayApp(QtCore.QObject):
    def __init__(self, app: QtWidgets.QApplication):
        super().__init__()
        self.app = app
        self.settings = QtCore.QSettings("MX-Linux", "mx-arch-updater")
        self.tray = QtWidgets.QSystemTrayIcon(self)
        self.menu = QtWidgets.QMenu()
        self.tray.setContextMenu(self.menu)
        self.tray.activated.connect(self._on_activated)
        self._notified_available = False
        self._state = {
            "counts": {"upgrade": 0, "new": 0, "remove": 0, "held": 0},
            "packages": [],
            "status": "idle",
        }

        self._setup_actions()
        self._setup_dbus()
        self._register_tray_service()
        self._update_ui()
        self.tray.show()

    def _setup_actions(self) -> None:
        self.action_view = QtGui.QAction("View && Upgrade", self.menu)
        self.action_view.triggered.connect(self.open_view)
        self.action_refresh = QtGui.QAction("Refresh", self.menu)
        self.action_refresh.triggered.connect(self.refresh)
        self.action_settings = QtGui.QAction("Settings", self.menu)
        self.action_settings.triggered.connect(self.open_settings)
        self.action_quit = QtGui.QAction("Quit", self.menu)
        self.action_quit.triggered.connect(self.app.quit)
        self.menu.addAction(self.action_view)
        self.menu.addAction(self.action_refresh)
        self.menu.addAction(self.action_settings)
        self.menu.addSeparator()
        self.menu.addAction(self.action_quit)

    def _setup_dbus(self) -> None:
        self.bus = QtDBus.QDBusConnection.systemBus()
        self.iface = QtDBus.QDBusInterface(
            SYSTEM_SERVICE_NAME,
            SYSTEM_OBJECT_PATH,
            SYSTEM_INTERFACE,
            self.bus,
        )
        self.bus.connect(
            SYSTEM_SERVICE_NAME,
            SYSTEM_OBJECT_PATH,
            SYSTEM_INTERFACE,
            "stateChanged",
            self,
            "_on_state_changed",
        )
        self.poll_timer = QtCore.QTimer(self)
        self.poll_timer.timeout.connect(self.poll_state)
        self.poll_timer.start(60 * 1000)
        self.refresh()

    def _register_tray_service(self) -> None:
        session_bus = QtDBus.QDBusConnection.sessionBus()
        if not session_bus.isConnected():
            return
        session_bus.registerService(TRAY_SERVICE_NAME)
        service = TrayService(self)
        session_bus.registerObject(
            TRAY_OBJECT_PATH,
            TRAY_INTERFACE,
            service,
            QtDBus.QDBusConnection.ExportAllSlots,
        )
        self._tray_service = service

    def refresh(self) -> None:
        if self.iface.isValid():
            self.iface.call("Refresh")
        else:
            self._update_ui()

    def poll_state(self) -> None:
        if not self.iface.isValid():
            return
        reply = self.iface.call("GetState")
        if reply.type() != QtDBus.QDBusMessage.ReplyMessage:
            return
        payload = reply.arguments()[0]
        self._on_state_changed(payload)

    @QtCore.Slot(str)
    def _on_state_changed(self, payload: str) -> None:
        try:
            self._state = json.loads(payload)
        except json.JSONDecodeError:
            return
        self._update_ui()

    def _icon_path(self, available: bool) -> Path:
        theme = str(self.settings.value("Settings/icon_theme", "wireframe-dark"))
        if theme not in ICON_THEMES:
            theme = "wireframe-dark"
        name = "updates-available.svg" if available else "up-to-date.svg"
        return icon_path(theme, name)

    def _update_ui(self) -> None:
        self.settings.sync()
        counts = self._state.get("counts", {})
        upgrades = int(counts.get("upgrade", 0))
        available = upgrades > 0
        autohide = str(self.settings.value("Settings/auto_hide", "false")).lower() in ("true", "1", "yes")
        icon_path = self._icon_path(available)
        if icon_path.exists():
            self.tray.setIcon(QtGui.QIcon(str(icon_path)))
        tooltip = (
            f"Upgrades: {counts.get('upgrade', 0)}\n"
            f"New: {counts.get('new', 0)}\n"
            f"Remove: {counts.get('remove', 0)}\n"
            f"Held: {counts.get('held', 0)}"
        )
        self.tray.setToolTip(tooltip)
        if autohide and not available:
            self.tray.hide()
        else:
            self.tray.show()
        notify = str(self.settings.value("Settings/notify", "true")).lower() in ("true", "1", "yes")
        if notify and available and not self._notified_available:
            self.tray.showMessage("Updates Available", tooltip, self.tray.icon())
            self._notified_available = True
        if not available:
            self._notified_available = False

    def _on_activated(self, reason: QtWidgets.QSystemTrayIcon.ActivationReason) -> None:
        if reason == QtWidgets.QSystemTrayIcon.Trigger:
            self.open_view()
        elif reason == QtWidgets.QSystemTrayIcon.MiddleClick:
            self.launch_helper()

    def open_view(self) -> None:
        self._launch_bin("updater-view-and-upgrade")

    def open_settings(self) -> None:
        self._launch_bin("updater-settings")

    def launch_helper(self) -> None:
        helper = self.settings.value("Settings/helper", "paru")
        helper = helper if helper else "paru"
        if shutil.which(helper):
            subprocess.Popen([helper])
        else:
            self.tray.showMessage("Helper not found", f"Could not locate {helper}")

    def _launch_bin(self, name: str) -> None:
        root = Path(os.environ.get("MX_ARCH_UPDATER_PATH", "."))
        path = root / "bin" / name
        if path.exists():
            subprocess.Popen([str(path)])


def main() -> int:
    parser = argparse.ArgumentParser(description="MX Arch Updater tray")
    parser.add_argument("--autostart", action="store_true")
    args = parser.parse_args()

    if os.geteuid() == 0:
        sys.stderr.write("ERROR: Do not run the tray as root.\n")
        return 1

    if args.autostart:
        s = QtCore.QSettings("MX-Linux", "mx-arch-updater")
        enabled = str(s.value("Settings/start_at_login", "true")).lower() in ("true", "1", "yes")
        if not enabled:
            return 0

    app = QtWidgets.QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)
    TrayApp(app)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

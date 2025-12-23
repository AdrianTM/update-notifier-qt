#!/usr/bin/env python3

import os
import sys

from PySide6 import QtCore, QtWidgets, QtDBus

from common import ICON_THEMES, UPGRADE_MODES


SETTINGS_SERVICE_NAME = "org.mxlinux.UpdaterSettings"
SETTINGS_OBJECT_PATH = "/org/mxlinux/UpdaterSettings"
SETTINGS_INTERFACE = "org.mxlinux.UpdaterSettings"


class SettingsService(QtCore.QObject):
    settingsChanged = QtCore.Signal(str, str)

    def __init__(self, dialog: "SettingsDialog") -> None:
        super().__init__()
        self.dialog = dialog

    @QtCore.Slot(str, result=str)
    def Get(self, key: str) -> str:
        return str(self.dialog.settings.value(key, ""))

    @QtCore.Slot(str, str)
    def Set(self, key: str, value: str) -> None:
        self.dialog.settings.setValue(key, value)
        self.dialog.settings.sync()
        self.settingsChanged.emit(key, value)


class SettingsDialog(QtWidgets.QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("MX Arch Updater Settings")
        self.resize(480, 300)
        self.settings = QtCore.QSettings("MX-Linux", "mx-arch-updater")
        self._register_settings_service()
        self._build_ui()
        self._load()

    def _register_settings_service(self) -> None:
        session_bus = QtDBus.QDBusConnection.sessionBus()
        if not session_bus.isConnected():
            return
        session_bus.registerService(SETTINGS_SERVICE_NAME)
        service = SettingsService(self)
        session_bus.registerObject(
            SETTINGS_OBJECT_PATH,
            SETTINGS_INTERFACE,
            service,
            QtDBus.QDBusConnection.ExportAllSlots | QtDBus.QDBusConnection.ExportAllSignals,
        )
        self._settings_service = service

    def _build_ui(self) -> None:
        self.icon_theme = QtWidgets.QComboBox()
        self.icon_theme.addItems(ICON_THEMES)
        self.auto_hide = QtWidgets.QCheckBox("Hide tray icon when no updates")
        self.notify = QtWidgets.QCheckBox("Notify when updates are available")
        self.start_login = QtWidgets.QCheckBox("Start at login")
        self.upgrade_mode = QtWidgets.QComboBox()
        self.upgrade_mode.addItems(UPGRADE_MODES)
        self.helper = QtWidgets.QLineEdit()
        self.helper.setPlaceholderText("paru")

        form = QtWidgets.QFormLayout()
        form.addRow("Icon theme", self.icon_theme)
        form.addRow("Auto hide", self.auto_hide)
        form.addRow("Notifications", self.notify)
        form.addRow("Start at login", self.start_login)
        form.addRow("Upgrade mode", self.upgrade_mode)
        form.addRow("Helper", self.helper)

        buttons = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.Save | QtWidgets.QDialogButtonBox.Cancel
        )
        buttons.accepted.connect(self.save)
        buttons.rejected.connect(self.reject)

        layout = QtWidgets.QVBoxLayout(self)
        layout.addLayout(form)
        layout.addStretch(1)
        layout.addWidget(buttons)

    def _load(self) -> None:
        self.icon_theme.setCurrentText(self.settings.value("Settings/icon_theme", "wireframe-dark"))
        self.auto_hide.setChecked(self._bool("Settings/auto_hide", False))
        self.notify.setChecked(self._bool("Settings/notify", True))
        self.start_login.setChecked(self._bool("Settings/start_at_login", True))
        self.upgrade_mode.setCurrentText(self.settings.value("Settings/upgrade_mode", "basic"))
        self.helper.setText(self.settings.value("Settings/helper", "paru"))

    def _bool(self, key: str, default: bool) -> bool:
        return str(self.settings.value(key, default)).lower() in ("true", "1", "yes")

    def save(self) -> None:
        self.settings.setValue("Settings/icon_theme", self.icon_theme.currentText())
        self.settings.setValue("Settings/auto_hide", self.auto_hide.isChecked())
        self.settings.setValue("Settings/notify", self.notify.isChecked())
        self.settings.setValue("Settings/start_at_login", self.start_login.isChecked())
        self.settings.setValue("Settings/upgrade_mode", self.upgrade_mode.currentText())
        self.settings.setValue("Settings/helper", self.helper.text().strip())
        self.settings.sync()
        self.accept()


def main() -> int:
    if os.geteuid() == 0:
        sys.stderr.write("ERROR: Do not run settings UI as root.\n")
        return 1
    app = QtWidgets.QApplication(sys.argv)
    dialog = SettingsDialog()
    dialog.exec()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

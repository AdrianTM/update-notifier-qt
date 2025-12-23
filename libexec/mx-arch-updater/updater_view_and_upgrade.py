#!/usr/bin/env python3

import json
import os
import subprocess
import sys

from PySide6 import QtCore, QtWidgets, QtDBus

from common import helper_path
SYSTEM_SERVICE_NAME = "org.mxlinux.UpdaterSystemMonitor"
SYSTEM_OBJECT_PATH = "/org/mxlinux/UpdaterSystemMonitor"
SYSTEM_INTERFACE = "org.mxlinux.UpdaterSystemMonitor"


class ViewAndUpgrade(QtWidgets.QDialog):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("MX Arch Updater")
        self.resize(680, 420)
        self.settings = QtCore.QSettings("MX-Linux", "mx-arch-updater")
        self.bus = QtDBus.QDBusConnection.systemBus()
        self.iface = QtDBus.QDBusInterface(
            SYSTEM_SERVICE_NAME,
            SYSTEM_OBJECT_PATH,
            SYSTEM_INTERFACE,
            self.bus,
        )
        self._build_ui()
        self.refresh()

    def _build_ui(self) -> None:
        self.counts_label = QtWidgets.QLabel()
        self.list_widget = QtWidgets.QListWidget()
        self.list_widget.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)

        self.button_refresh = QtWidgets.QPushButton("Refresh")
        self.button_upgrade = QtWidgets.QPushButton("Upgrade")
        self.button_close = QtWidgets.QPushButton("Close")

        self.button_refresh.clicked.connect(self.refresh)
        self.button_upgrade.clicked.connect(self.upgrade)
        self.button_close.clicked.connect(self.close)

        button_row = QtWidgets.QHBoxLayout()
        button_row.addStretch(1)
        button_row.addWidget(self.button_refresh)
        button_row.addWidget(self.button_upgrade)
        button_row.addWidget(self.button_close)

        layout = QtWidgets.QVBoxLayout(self)
        layout.addWidget(self.counts_label)
        layout.addWidget(self.list_widget)
        layout.addLayout(button_row)

    def refresh(self) -> None:
        if not self.iface.isValid():
            self.counts_label.setText("System monitor is not available.")
            return
        reply = self.iface.call("GetState")
        if reply.type() != QtDBus.QDBusMessage.ReplyMessage:
            self.counts_label.setText("Unable to query system monitor.")
            return
        payload = reply.arguments()[0]
        try:
            state = json.loads(payload)
        except json.JSONDecodeError:
            self.counts_label.setText("Received invalid state from monitor.")
            return
        counts = state.get("counts", {})
        self.counts_label.setText(
            f"Upgrades: {counts.get('upgrade', 0)} | "
            f"New: {counts.get('new', 0)} | "
            f"Remove: {counts.get('remove', 0)} | "
            f"Held: {counts.get('held', 0)}"
        )
        self.list_widget.clear()
        for item in state.get("packages", []):
            self.list_widget.addItem(item)

    def upgrade(self) -> None:
        upgrade_mode = self.settings.value("Settings/upgrade_mode", "basic")
        script = helper_path("updater_upgrade")
        if not script.exists():
            QtWidgets.QMessageBox.warning(self, "Upgrade", "Upgrade helper script not found.")
            return
        command = ["pkexec", str(script), "--mode", str(upgrade_mode)]
        progress = QtWidgets.QProgressDialog("Upgrading packages...", "Cancel", 0, 0, self)
        progress.setWindowModality(QtCore.Qt.WindowModal)
        progress.setCancelButton(None)
        progress.show()
        process = QtCore.QProcess(self)
        process.start(command[0], command[1:])
        process.finished.connect(lambda *_: self._on_upgrade_finished(progress))

    def _on_upgrade_finished(self, progress: QtWidgets.QProgressDialog) -> None:
        progress.close()
        self.refresh()


def main() -> int:
    if os.geteuid() == 0:
        sys.stderr.write("ERROR: Do not run the UI as root.\n")
        return 1
    app = QtWidgets.QApplication(sys.argv)
    dialog = ViewAndUpgrade()
    dialog.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

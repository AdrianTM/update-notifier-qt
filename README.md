# MX Arch Updater (Qt)

Qt6/PySide6-based system tray updater for Arch Linux, modeled after the MX Updater layout.

## Components

- `bin/updater-systray`: Session tray application
- `bin/updater-system-monitor`: System D-Bus monitor service (root)
- `bin/updater-view-and-upgrade`: View/upgrade dialog
- `bin/updater-settings`: Settings dialog
- `dbus/`: D-Bus service files
- `icons/`: Icon themes

## Quick Run (development)

1. Start the system monitor as root (in another terminal):
   `sudo ./bin/updater-system-monitor`
2. Start the tray icon:
   `./bin/updater-systray`
3. Open dialogs from the tray menu.

## Notes

- `pacman -Qu` is used to detect available updates.
- Upgrade operations are performed via `pkexec pacman -Syu`.
- QSettings key namespace: `MX-Linux/mx-arch-updater`.

## Arch Packaging

- `PKGBUILD` installs to `/usr/bin` and `/usr/share/mx-arch-updater`.
- System service: `systemd/mx-arch-updater-monitor.service`
- User service: `systemd/mx-arch-updater-tray.service`

# MX Arch Updater (Qt)

Qt6/PySide6-based system tray updater for Arch Linux, modeled after the MX Updater layout.

## Components

- `mx-updater-systray`: Session tray application
- `mx-updater-system-monitor`: System D-Bus monitor service (root)
- `mx-updater-view-and-upgrade`: View/upgrade dialog
- `mx-updater-settings`: Settings dialog
- `dbus/`: D-Bus service files
- `icons/`: Icon themes

## Quick Run (development)

1. Build the project: `./build.sh`
2. Start the system monitor as root (in another terminal):
   `sudo ./build/mx-updater-system-monitor`
3. Start the tray icon:
   `./build/mx-updater-systray`
4. Open dialogs from the tray menu.

## Notes

- `pacman -Qu` is used to detect available updates.
- Upgrade operations are performed via `pkexec pacman -Syu`.
- QSettings key namespace: `MX-Linux/mx-arch-updater`.

## Arch Packaging

- `PKGBUILD` installs to `/usr/bin` and `/usr/share/mx-arch-updater`.
- System service: `systemd/mx-arch-updater-monitor.service`
- User service: `systemd/mx-arch-updater-tray.service`

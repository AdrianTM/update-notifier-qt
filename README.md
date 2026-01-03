# Update Notifier Qt

Qt6-based system tray update notifier for Arch Linux, modeled after the MX Updater layout.

## Components

- `update-notifier-systray`: Session tray application
- `update-notifier-system-monitor`: System D-Bus monitor service (root)
- `update-notifier-view-and-upgrade`: View/upgrade dialog
- `update-notifier-settings`: Settings dialog
- `dbus/`: D-Bus service files
- `icons/`: Icon themes

## Quick Run (development)

1. Build the project: `./build.sh`
2. Start the system monitor as root (in another terminal):
   `sudo ./build/update-notifier-system-monitor`
3. Start the tray icon:
   `./build/update-notifier-systray`
4. Open dialogs from the tray menu.

## Notes

- `pacman -Qu` is used to detect available updates.
- Upgrade operations are performed via `pkexec pacman -Syu`.
- QSettings key namespace: `MX-Linux/update-notifier-qt`.

## Arch Packaging

- `PKGBUILD` installs to `/usr/bin` and `/usr/share/update-notifier-qt`.
- System service: `systemd/update-notifier-monitor.service`
- User service: `systemd/update-notifier-tray.service`

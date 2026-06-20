# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Standard build (Release mode with Ninja)
./build.sh

# Debug build
./build.sh -d

# Clean build
./build.sh --clean

# Build Arch Linux package
./build.sh --arch
```

Executables are placed in `build/`.

## Development Setup

Run the system monitor as root in one terminal, then start the tray app in another:

```bash
sudo ./build/update-notifier-system-monitor
./build/update-notifier-systray
```

## Architecture

This is a Qt6-based system tray update notifier for Arch Linux with three executables communicating via D-Bus:

### Components

1. **update-notifier-system-monitor** (runs as root)
   - System D-Bus service: `org.mxlinux.UpdateNotifierSystemMonitor`
   - Periodically runs `pacman -Qu` and optional AUR helper queries
   - Maintains state in `/var/lib/update-notifier-qt/state.json`
   - Emits `stateChanged` and `summaryChanged` D-Bus signals

2. **update-notifier-systray** (user session)
   - Session D-Bus service: `org.mxlinux.UpdateNotifierTrayIcon`
   - Connects to system monitor via D-Bus, polls for state changes
   - Shows tray icon and context menu
   - Embeds SettingsDialog and HistoryDialog
   - Contains TrayService (D-Bus) and SettingsService (settings sync)

3. **update-notifier-view-and-upgrade** (launched from tray)
   - Shows pending updates in a tree view
   - Runs `sudo pacman -S` for upgrades

### Key Classes

- `SystemMonitor`: Core update detection logic, pacman/AUR queries, state management
- `TrayApp`: Main tray application with menu, D-Bus connections, and dialog management
- `TrayService` / `SettingsService`: D-Bus adapters for external control
- `ViewAndUpgrade`: Update list and upgrade dialog
- `SettingsDialog`: Configuration UI with icon theme selection

### D-Bus Interfaces

- System bus: `org.mxlinux.UpdateNotifierSystemMonitor` (state queries, refresh, settings)
- Session bus: `org.mxlinux.UpdateNotifierTrayIcon` (quit, refresh, show dialogs)

### Settings

QSettings namespace: `MX-Linux/update-notifier-qt`

State file: `/var/lib/update-notifier-qt/state.json` (written by system monitor, read by tray)

## Code Style

- C++20 with Qt6 (Core, Widgets, DBus, Svg)
- `QT_NO_CAST_FROM_ASCII` and `QT_NO_CAST_TO_ASCII` are enabled - use `QStringLiteral()` for string literals
- Version derived from `PKGBUILD` via `version.h.in`

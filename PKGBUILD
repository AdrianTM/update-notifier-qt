pkgname=mx-arch-updater
pkgver=0.1.0
pkgrel=1
pkgdesc="MX Updater tray for Arch Linux"
arch=("any")
license=("GPL")
depends=("python" "pyside6" "qt6-base" "dbus" "polkit" "pacman")
source=()

# Build in build/ directory and place packages there
export PKGDEST="$PWD/build"

build() {
  :
}

package() {
  install -d "${pkgdir}/usr/bin"
  install -d "${pkgdir}/usr/libexec/mx-arch-updater"
  install -d "${pkgdir}/usr/lib/mx-arch-updater"
  install -d "${pkgdir}/usr/share/mx-arch-updater"
  install -d "${pkgdir}/usr/share/doc/${pkgname}"
  install -d "${pkgdir}/usr/share/dbus-1/system-services"
  install -d "${pkgdir}/usr/share/dbus-1/services"
  install -d "${pkgdir}/usr/share/polkit-1/actions"
  install -d "${pkgdir}/usr/lib/systemd/system"
  install -d "${pkgdir}/usr/lib/systemd/user"

  install -Dm755 ../bin/updater-systray "${pkgdir}/usr/bin/updater-systray"
  install -Dm755 ../bin/updater-system-monitor "${pkgdir}/usr/bin/updater-system-monitor"
  install -Dm755 ../bin/updater-view-and-upgrade "${pkgdir}/usr/bin/updater-view-and-upgrade"
  install -Dm755 ../bin/updater-settings "${pkgdir}/usr/bin/updater-settings"

  install -Dm755 ../libexec/mx-arch-updater/updater_systray.py "${pkgdir}/usr/libexec/mx-arch-updater/updater_systray.py"
  install -Dm755 ../libexec/mx-arch-updater/updater_system_monitor.py "${pkgdir}/usr/libexec/mx-arch-updater/updater_system_monitor.py"
  install -Dm755 ../libexec/mx-arch-updater/updater_view_and_upgrade.py "${pkgdir}/usr/libexec/mx-arch-updater/updater_view_and_upgrade.py"
  install -Dm755 ../libexec/mx-arch-updater/updater_settings.py "${pkgdir}/usr/libexec/mx-arch-updater/updater_settings.py"
  install -Dm644 ../libexec/mx-arch-updater/common.py "${pkgdir}/usr/libexec/mx-arch-updater/common.py"
  install -Dm644 ../libexec/mx-arch-updater/__init__.py "${pkgdir}/usr/libexec/mx-arch-updater/__init__.py"

  install -Dm755 ../lib/mx-arch-updater/updater_reload "${pkgdir}/usr/lib/mx-arch-updater/updater_reload"
  install -Dm755 ../lib/mx-arch-updater/updater_upgrade "${pkgdir}/usr/lib/mx-arch-updater/updater_upgrade"
  install -Dm755 ../lib/mx-arch-updater/updater_count "${pkgdir}/usr/lib/mx-arch-updater/updater_count"
  install -Dm755 ../lib/mx-arch-updater/updater_list "${pkgdir}/usr/lib/mx-arch-updater/updater_list"

  install -Dm644 ../dbus/org.mxlinux.UpdaterSystemMonitor.service "${pkgdir}/usr/share/dbus-1/system-services/org.mxlinux.UpdaterSystemMonitor.service"
  install -Dm644 ../dbus/org.mxlinux.UpdaterSystemTrayIcon.service "${pkgdir}/usr/share/dbus-1/services/org.mxlinux.UpdaterSystemTrayIcon.service"
  install -Dm644 ../dbus/org.mxlinux.UpdaterSettings.service "${pkgdir}/usr/share/dbus-1/services/org.mxlinux.UpdaterSettings.service"

  install -Dm644 ../polkit/org.mxlinux.mx-arch-updater.policy "${pkgdir}/usr/share/polkit-1/actions/org.mxlinux.mx-arch-updater.policy"

  install -Dm644 ../systemd/mx-arch-updater-monitor.service "${pkgdir}/usr/lib/systemd/system/mx-arch-updater-monitor.service"
  install -Dm644 ../systemd/mx-arch-updater-tray.service "${pkgdir}/usr/lib/systemd/user/mx-arch-updater-tray.service"

  cp -r ../icons "${pkgdir}/usr/share/mx-arch-updater/"
  install -Dm644 ../README.md "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}

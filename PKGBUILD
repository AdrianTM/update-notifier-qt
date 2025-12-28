pkgname=mx-arch-updater
pkgver=25.12
pkgrel=1
pkgdesc="MX Updater tray for Arch Linux"
arch=("x86_64")
license=("GPL")
depends=("qt6-base" "qt6-svg" "dbus" "polkit" "pacman")
makedepends=("cmake" "ninja" "qt6-tools")
install=mx-arch-updater.install
source=("src/common.cpp"
         "src/common.h"
         "src/settings_dialog.cpp"
         "src/settings_dialog.h"
         "src/settings_service.cpp"
         "src/settings_service.h"
         "src/system_monitor.cpp"
         "src/system_monitor.h"
         "src/monitor_main.cpp"
         "src/tray_app.cpp"
         "src/tray_app.h"
         "src/tray_service.cpp"
         "src/tray_service.h"
         "src/systray_main.cpp"
         "src/view_and_upgrade.cpp"
         "src/view_and_upgrade.h"
         "src/view_main.cpp"
         "src/history_dialog.cpp"
         "src/history_dialog.h")
sha256sums=('SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP')


build() {
  mkdir -p build
  cd build

  # Only run CMake configuration if needed (incremental build optimization)
  if [ ! -f build.ninja ] || [ ../CMakeLists.txt -nt build.ninja ]; then
    echo "Configuring with CMake..."
    cmake -G Ninja .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  else
    echo "Build configuration is up to date, skipping CMake configuration."
  fi

  # Build with Ninja for faster incremental builds
  ninja
}

package() {
  cd "$srcdir/build"
  DESTDIR="${pkgdir}" ninja install

  # Create symlink to enable tray service globally (package-managed)
  install -dm755 "${pkgdir}/usr/lib/systemd/user/graphical-session.target.wants"
  ln -s ../mx-arch-updater-tray.service \
    "${pkgdir}/usr/lib/systemd/user/graphical-session.target.wants/mx-arch-updater-tray.service"
}

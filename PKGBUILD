pkgname=update-notifier-qt
pkgver=25.12
pkgrel=1
pkgdesc="Qt-based update notifier tray for Arch Linux"
arch=("x86_64")
license=("GPL")
depends=("qt6-base" "qt6-svg" "dbus" "polkit" "pacman")
makedepends=("cmake" "ninja" "qt6-tools")
install=update-notifier-qt.install
source=()
sha256sums=()


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
  ln -s ../update-notifier-tray.service \
    "${pkgdir}/usr/lib/systemd/user/graphical-session.target.wants/update-notifier-tray.service"
}

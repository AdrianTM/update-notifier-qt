pkgname=mx-arch-updater
pkgver=0.1.0
pkgrel=1
pkgdesc="MX Updater tray for Arch Linux"
arch=("x86_64")
license=("GPL")
depends=("qt6-base" "qt6-svg" "dbus" "polkit" "pacman")
makedepends=("cmake" "qt6-tools")
source=("src/common.cpp"
         "src/common.h"
         "src/settings_dialog.cpp"
         "src/settings_dialog.h"
         "src/settings_service.cpp"
         "src/settings_service.h"
         "src/settings_main.cpp"
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
         "src/view_main.cpp")
sha256sums=('787f902d1e3124d1789e196bbe9089998c6eb5ce5377b1243db0bac084eb4446'
            '01d6002782516c67d2741151fb075cc39b74e906a6911ebabc4acb4683851791'
            '6228cf92e328f91eae33cc307246b4230d2dbe449482836ee34b794bbcb07651'
            '70fff7b0582de483d1b7f8c1748ea57292239b7eaafe1b7cc2c9ed05c8d731ae'
            '65d053f6dfe3868742b98aaa3c5e5d636f0116273c1e614311fe886fd7f376e8'
            '53431663180982d9aa8f91f5eaf7d9242753b7c6b132b9ff8daf357f0dc37285'
            '5718b91142cb1c31526b2d6ca39a71b2c3e34ac9c61f0f7ce53e9f9f6f8392da'
            'ed638c416574422516b0db4b15a0b466ff6fb8f230eb35b076dd534d12739129'
            '01992c4fc5ac9558d5250a59f5108d7c850e79190cb006ec0b465193fa9a8532'
            '34640eda7a18162b99c62fe2841c1e7dc99a6292cd18af98a4d9d87ceabae17f'
            '7e90439de3910a4043deaafe0e33504d0c5998fbcb75758c8354c927773e12b9'
            'dd9b7aec2bd40e9bdd87debe0f50f475039dbc153d783af8ee32f2a4652568cf'
            'a1ba74b794ae9be37c541a707ce7643233b4220767d6db75085c17f6043c3a61'
            '77802a849b0411fa6fe161eef0f3c87ab9efc9b9a82060317bacc2eb63b09869'
            '7b7e142ab9a28ad1607caae1a4921a6b143998568b5164a6bfd820a72b42d9e9'
            'ad10c832a5f05e69637c8cb2e2d1d2d6347bcf860b828552d1778f3b78682b2d'
            'e19b097dca158ec639ac8e74cbfe56ec187d00ad75d36b528b1ef210958dffbb'
            '4ef45f1e06ae63de74500c640b2f51f66b0cd988631b92bc7b597abab882e1ca')


build() {
  mkdir -p build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
  make
}

package() {
  cd "$srcdir/build"
  make DESTDIR="${pkgdir}" install

  # Install helper scripts
  install -Dm755 "$srcdir/lib/mx-arch-updater/updater_reload" "${pkgdir}/usr/lib/mx-arch-updater/updater_reload"
  install -Dm755 "$srcdir/lib/mx-arch-updater/updater_upgrade" "${pkgdir}/usr/lib/mx-arch-updater/updater_upgrade"
  install -Dm755 "$srcdir/lib/mx-arch-updater/updater_count" "${pkgdir}/usr/lib/mx-arch-updater/updater_count"
  install -Dm755 "$srcdir/lib/mx-arch-updater/updater_list" "${pkgdir}/usr/lib/mx-arch-updater/updater_list"

  # Install documentation
  install -Dm644 "$srcdir/README.md" "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}

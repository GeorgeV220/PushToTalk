### PKGBUILD
pkgname=pttcpp
gitrepo="https://github.com/GeorgeV220/PushToTalk.git"
pkgver=1.0
pkgrel=1
arch=('x86_64')
license=('GPL')
depends=('gtk3' 'openal' 'mpg123' 'cmake' 'gcc' 'pkgconf')
makedepends=('git')
source=("git+$gitrepo")
md5sums=('SKIP')

build() {
  cd "$srcdir/PushToTalk"
  mkdir -p build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make
}

package() {
  install -Dm755 "$srcdir/PushToTalk/build/pttcpp" "$pkgdir/usr/bin/pttcpp"
  install -Dm644 "$srcdir/PushToTalk/pttcpp.service" "$pkgdir/usr/lib/systemd/user/pttcpp.service"
}

post_install() {
  echo "Enabling pttcpp systemd user service..."
  systemctl --user enable pttcpp.service
  echo "Run pttcpp --detect to find your device and then pttcpp --gui to set up your settings, after than start the service or restart the system"
}

post_remove() {
  echo "Disabling pttcpp systemd user service..."
  systemctl --user disable pttcpp.service
}

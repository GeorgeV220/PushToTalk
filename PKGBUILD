# Maintainer: GeorgeV22 <email: contact at georgev22 dot com>
pkgname=pushtotalk
pkgver=1.0
pkgrel=1
pkgdesc="Push-to-Talk voice communication tool"
arch=('x86_64')
url="https://example.com"
license=('MIT')
depends=('gtk3' 'libpulse' 'openal' 'zlib' 'mpg123' 'libappindicator-gtk3')
makedepends=('cmake')
install=pushtotalk.install
source=(
  "$pkgname::git+https://github.com/GeorgeV220/PushToTalk.git"
  "ptt-server.service.in"
  "ptt-client.service"
)
md5sums=('SKIP' 'SKIP' 'SKIP')

build() {
  cmake -S "$srcdir/$pkgname" -B "$srcdir/$pkgname/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$srcdir/$pkgname/build"
}

package() {
  install -Dm755 "$srcdir/$pkgname/build/ptt-client" "$pkgdir/usr/bin/ptt-client"
  install -Dm755 "$srcdir/$pkgname/build/ptt-server" "$pkgdir/usr/bin/ptt-server"

  install -Dm644 "$srcdir/ptt-server.service.in" "$pkgdir/usr/lib/systemd/system/ptt-server.service"
  install -Dm644 "$srcdir/ptt-client.service" "$pkgdir/usr/lib/systemd/user/ptt-client.service"
}

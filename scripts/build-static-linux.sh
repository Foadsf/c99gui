#!/bin/sh
# Build fully static Linux binaries of all four programs inside Alpine Linux (musl libc).
# X11 is linked statically too, so the GUI binaries run on any Linux distribution with an
# X server, without libX11 installed. Run inside an Alpine container from the repository root:
#   docker run --rm -v "$PWD":/src -w /src alpine:3.22 sh scripts/build-static-linux.sh build
set -eu
OUT=${1:-build}
ROOT=$(pwd)

apk add --no-cache build-base xorgproto libx11-dev libx11-static libxcb-dev libxcb-static \
    libxdmcp-dev xz file >/dev/null

# Alpine ships no static libXau (libxcb needs it), so build it from the X.org release.
# Version and SHA-512 are taken from Alpine's own main/libxau/APKBUILD.
XAU_VER=1.0.12
XAU_SHA512=4bbe8796f4a14340499d5f75046955905531ea2948944dfc3d6069f8b86c1710042bfc7918d459320557883e6631359d48e6173c69c62ff572314e864ff97c5e
if [ ! -f /usr/lib/libXau.a ]; then
    cd /tmp
    wget -q "https://www.x.org/releases/individual/lib/libXau-$XAU_VER.tar.xz"
    echo "$XAU_SHA512  libXau-$XAU_VER.tar.xz" | sha512sum -c -
    tar xJf "libXau-$XAU_VER.tar.xz"
    cd "libXau-$XAU_VER"
    ./configure --prefix=/usr --enable-static --disable-shared >/dev/null
    make -s >/dev/null
    make -s install >/dev/null
fi
cd "$ROOT"

mkdir -p "$OUT"
FLAGS="-std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror"
X11_STATIC="-lX11 -lxcb -lXau -lXdmcp"

cc $FLAGS -static src/tui/c99gui.c -o "$OUT/c99gui-tui"
for p in pixel vector suite; do
    cc $FLAGS -static "src/$p/gui.c" $X11_STATIC -o "$OUT/c99gui-$p"
done

# Drop the debug info that comes in with the static musl/X11 libraries (about 10x the size otherwise).
strip "$OUT"/c99gui-tui "$OUT"/c99gui-pixel "$OUT"/c99gui-vector "$OUT"/c99gui-suite

# Every binary must be static, must have the X11 backend compiled in (GUIs), and must start.
status=0
for b in "$OUT"/c99gui-tui "$OUT"/c99gui-pixel "$OUT"/c99gui-vector "$OUT"/c99gui-suite; do
    info=$(file "$b")
    echo "$info"
    case "$info" in
        *"statically linked"*|*"static-pie linked"*) ;;
        *) echo "NOT STATIC: $b" >&2; status=1 ;;
    esac
    "$b" --version || { echo "DOES NOT RUN: $b" >&2; status=1; }
done
for p in pixel vector suite; do
    grep -q 'cannot connect to X11 display' "$OUT/c99gui-$p" || { echo "NO X11 BACKEND: c99gui-$p" >&2; status=1; }
done
exit $status

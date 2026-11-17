#!/bin/sh
# Builds NokiaLink with the pretend phone wired in (-DNOKIALINK_FAKEPHONE,
# in build-fake) and launches it in Mini vMac, so the Phone window shows
# status fetched through the link from the pretend phone. For looking, not
# a pass/fail test; quit Mini vMac when done.
#
# Usage: tests/run-fake.sh
#   RETRO68  Retro68 build folder (default ~/Retro68-build)

set -u
cd "$(dirname "$0")/.." || exit 2
OUT=build-fake
RETRO68=${RETRO68:-$HOME/Retro68-build}

if pgrep -x minivmac > /dev/null; then
    echo "Mini vMac is already running: quit it first."; exit 2
fi
mkdir -p "$OUT"
cmake -S . -B "$OUT" -DNOKIALINK_FAKEPHONE=ON \
      -DCMAKE_TOOLCHAIN_FILE="$RETRO68/build-target/cmake/retro68.toolchain.cmake" \
      > "$OUT/cmake.log" 2>&1 || { cat "$OUT/cmake.log"; exit 2; }
cmake --build "$OUT" > "$OUT/build.log" 2>&1 \
    || { grep -E "error|warning" "$OUT/build.log"; echo "build failed"; exit 2; }

echo "Launching in Mini vMac; quit it when you have seen the Phone window."
"$RETRO68/toolchain/bin/LaunchAPPL" --timeout "${FAKE_TIMEOUT:-600}" "$OUT/NokiaLink.APPL"

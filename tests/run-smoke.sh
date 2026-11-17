#!/bin/sh
# Builds NokiaLink with the smoke test (-DNOKIALINK_SMOKE, in build-smoke),
# runs it in Mini vMac with LaunchAPPL, and checks its output. Exit 0 only
# if the log ends "FAILURES: 0" then "DONE". Adapted from MacMoney's.
#
# Usage: tests/run-smoke.sh [build folder]   (default: build-smoke)
#   RETRO68         Retro68 build folder (default ~/Retro68-build)
#   LAUNCHAPPL_CFG  a LaunchAPPL settings file (default ~/.LaunchAPPL.cfg)
# Leave the Mini vMac it starts in front: it stops when another app is in
# front and the test then times out.

set -u
cd "$(dirname "$0")/.." || exit 2
OUT=${1:-build-smoke}
RETRO68=${RETRO68:-$HOME/Retro68-build}
TIMEOUT=${SMOKE_TIMEOUT:-300}

if pgrep -x minivmac > /dev/null; then
    echo "Mini vMac is already running: quit it first."; exit 2
fi
mkdir -p "$OUT"
cmake -S . -B "$OUT" -DNOKIALINK_SMOKE=ON \
      -DCMAKE_TOOLCHAIN_FILE="$RETRO68/build-target/cmake/retro68.toolchain.cmake" \
      > "$OUT/cmake.log" 2>&1 || { cat "$OUT/cmake.log"; exit 2; }
cmake --build "$OUT" > "$OUT/build.log" 2>&1 \
    || { grep -E "error|warning" "$OUT/build.log"; echo "build failed"; exit 2; }
grep -E "warning:" "$OUT/build.log"

"$RETRO68/toolchain/bin/LaunchAPPL" --timeout "$TIMEOUT" \
    ${LAUNCHAPPL_CFG:+--config-file "$LAUNCHAPPL_CFG"} \
    "$OUT/NokiaLink.APPL" > "$OUT/smoke.log" 2>&1
status=$?
grep -v '^[.#]*$' "$OUT/smoke.log"

if [ $status -eq 0 ] && grep -qx "FAILURES: 0" "$OUT/smoke.log" \
   && [ "$(grep -v '^[.#]*$' "$OUT/smoke.log" | tail -1)" = "DONE" ]; then
    echo "smoke test: passed"; exit 0
fi
echo "smoke test: FAILED (LaunchAPPL exit $status)"; exit 1

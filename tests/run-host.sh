#!/bin/sh
# Builds and runs the host tests of the portable core (src/core) on the
# modern Mac, and builds the host tool. Output goes to build-host/.
#
# Usage: tests/run-host.sh

set -eu
cd "$(dirname "$0")/.."
B=build-host
mkdir -p "$B"
CFLAGS="-std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -O1 -g"

cc $CFLAGS -fsanitize=address,undefined \
    src/core/fbus.c src/core/link.c \
    tests/fakephone.c tests/test_core.c \
    -o "$B/test_core"
"$B/test_core"

cc $CFLAGS -fsanitize=address,undefined \
    src/store/nldate.c src/store/nlnumber.c src/store/nlusage.c src/store/nlvcard.c \
    tests/test_store.c \
    -o "$B/test_store"
"$B/test_store"

cc $CFLAGS \
    src/core/fbus.c src/core/link.c \
    host/port_posix.c host/nokialink-host.c \
    -o "$B/nokialink-host"
echo "built $B/nokialink-host"

python3 tests/ptyphone.py "$B/nokialink-host" "$B/pty-trace.txt"

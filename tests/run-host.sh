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

cc $CFLAGS -fsanitize=address,undefined \
    src/store/nltext.c tests/test_text.c \
    -o "$B/test_text"
"$B/test_text"

cc $CFLAGS -fsanitize=address,undefined \
    src/core/nlcal.c src/store/nltext.c src/store/nldate.c tests/test_cal.c \
    -o "$B/test_cal"
"$B/test_cal"

cc $CFLAGS -fsanitize=address,undefined \
    src/core/nlpbk.c src/store/nltext.c tests/test_pbk.c \
    -o "$B/test_pbk"
"$B/test_pbk"

cc $CFLAGS -fsanitize=address,undefined \
    src/core/nlgsm.c src/store/nltext.c src/store/nldate.c tests/test_gsm.c \
    -o "$B/test_gsm"
"$B/test_gsm"

cc $CFLAGS -fsanitize=address,undefined \
    src/core/nlsms.c src/core/nlgsm.c src/store/nltext.c src/store/nldate.c tests/test_sms.c \
    -o "$B/test_sms"
"$B/test_sms"

cc $CFLAGS -fsanitize=address,undefined \
    src/core/nlinfo.c src/store/nltext.c src/store/nldate.c tests/test_info.c \
    -o "$B/test_info"
"$B/test_info"

cc $CFLAGS \
    src/core/fbus.c src/core/link.c \
    host/port_posix.c host/nokialink-host.c \
    -o "$B/nokialink-host"
echo "built $B/nokialink-host"

python3 tests/ptyphone.py "$B/nokialink-host" "$B/pty-trace.txt"

#!/usr/bin/env python3
"""Tries the host tool end to end without a phone: a pretend phone on a
pseudo-terminal answers the modem commands and one "identify" request.
This exercises host/port_posix.c and host/nokialink-host.c; the protocol
itself is tested by test_core.c. Run by tests/run-host.sh.

Usage: ptyphone.py <path to nokialink-host> <trace file>"""
import os, pty, sys, time, select, subprocess, tty
m, s = pty.openpty(); tty.setraw(s); name = os.ttyname(s)
def frame(typ, block):
    f = bytes([0x1E, 0x0C, 0x00, typ, len(block) >> 8, len(block) & 255]) + block
    if len(block) & 1: f += b'\0'
    e = o = 0
    for i in range(0, len(f), 2): e ^= f[i]; o ^= f[i+1]
    return f + bytes([e, o])
p = subprocess.Popen([sys.argv[1], 'identify', name, sys.argv[2]], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
buf = b''; end = time.time() + 8; answered = False
while time.time() < end and p.poll() is None:
    r, _, _ = select.select([m], [], [], 0.05)
    if not r: continue
    buf += os.read(m, 256)
    while b'\r\n' in buf and buf.startswith(b'AT'):
        line, buf = buf.split(b'\r\n', 1); os.write(m, b'\r\nOK\r\n')
    i = buf.find(b'\x1e\x00\x0c\xd1')
    if i >= 0 and len(buf) >= i + 16 and not answered:
        answered = True
        os.write(m, frame(0x7F, bytes([0xD1, buf[i+12] & 0x0F])))
        os.write(m, frame(0xD2, b'\x01\x00\x00\x03V 04.80\n23-02-00\nNSE-5\n(c) NMP.' + bytes([1, 0x41])))
out = p.communicate()[0].decode()
print(out, end="")
ok = "NSE-5" in out and "OK to 3 of 3" in out and p.returncode == 0
print("host tool on a pseudo-terminal:", "passed" if ok else "FAILED")
sys.exit(0 if ok else 1)

# FBUS as NokiaLink speaks it

The link between the computer and the phone, as implemented in
`src/core/fbus.c` and `src/core/link.c`. Written from the Gammu
project's protocol documentation and published examples of frames.

**Not yet confirmed against the owner's phone** (15 November 2026).
The code is tested against a pretend phone built from this same
description, so the tests show that the code does what this page says,
not that the page is right. Phase 0's recordings of the real phone will
be checked against it.

## 1. Starting a session over the DLR-3P cable

| Step | What | Wait |
|---|---|---|
| 1 | Drop the control lines that power the cable | 1 second |
| 2 | Raise them | 1 second |
| 3 | At 19,200 baud, 8 data bits, no parity, send `AT` and a line end | 0.1 second, reading the reply |
| 4 | Send `AT&F` | 0.1 second |
| 5 | Send `AT*NOKIAFBUS` | 0.1 second |
| 6 | Change to 115,200 baud | |
| 7 | Send 32 bytes of `55`, then one byte `C1` | 1 second |

The phone starts as a modem; step 5 asks it to speak FBUS instead. A
line end is carriage return then line feed. The phone normally answers
each command with `OK`; NokiaLink counts these but carries on without
them.

## 2. A frame

Every frame, in either direction:

| Bytes | Meaning |
|---|---|
| `1E` | Start of a frame on a cable |
| 1 | Who it is for: `00` the phone, `0C` the computer |
| 1 | Who it is from |
| 1 | Message type (which subject: `03` contacts, `13` calendar, `14` text messages, `D1` identity, and so on) |
| 2 | Length of the body, high byte first |
| body | See below |
| 0 or 1 | A `00` to make the body an even length |
| 1 | Checksum 1: every byte at an even position (the first is position 0), XORed together |
| 1 | Checksum 2: the same for the odd positions |

The body of an ordinary frame is the message's bytes, then two more:

- **Frames to go:** how many frames of this message remain, counting
  this one. `01` is the last.
- **Sequence:** a number from 0 to 7 that goes up by one with each
  frame sent, then round again. `40` is added in the first frame of a
  message.

Example: the request for the phone's model and version, as the first
frame of a session.

    1E 00 0C D1 00 07  00 01 00 03 00  01 40  00  52 D5
    start to from type length  message  to-go seq pad checksums

## 3. Acknowledgements

Every frame is acknowledged by the other side with a frame of type
`7F` whose body is two bytes: the type being acknowledged, and its
sequence number without the `40`.

    1E 0C 00 7F 00 02  D1 00  CF 71      the phone acknowledging the frame above

Two exceptions: acknowledgements are not themselves acknowledged, and
frames of type `00` (the phone's debug trace) are never acknowledged.

A frame that isn't acknowledged is sent again. NokiaLink waits half a
second, and sends a frame at most three times before reporting that
the phone isn't answering.

## 4. Long messages

A message of more than 120 bytes is sent as several frames of up to
120 message bytes each. "Frames to go" counts down to 1; only the
first frame's sequence byte carries `40`. Each frame is acknowledged
before the next is sent. The receiver joins the pieces.

## 5. A request and its reply

1. The computer sends a request frame.
2. The phone acknowledges it.
3. The phone sends its reply, usually with the same or the next
   message type.
4. The computer acknowledges the reply.

The phone also sends messages nobody asked for: an incoming call, a new
text. They are acknowledged in the same way.

## 6. How NokiaLink deals with trouble

| Trouble | What happens |
|---|---|
| A checksum is wrong | The frame is thrown away and not acknowledged; the phone sends it again |
| Bytes arrive that aren't a frame | Skipped until the next `1E`; counted |
| The phone repeats a frame (it missed the acknowledgement) | Acknowledged again, not delivered twice. A repeat is a frame identical to the one before it, within three seconds |
| A later piece of a long message arrives out of step | Ignored; not acknowledged |
| A message is longer than NokiaLink's buffer | Still acknowledged so the phone isn't held up; reported as too long, not delivered |
| A frame claims a body over 512 bytes | Refused; counted |

The link keeps counts of all of these. They are the reliability figures
that Phase 2 measures on the real Plus.

## 7. The code

| File | Holds |
|---|---|
| `src/core/fbus.h`, `fbus.c` | Building frames; reading frames a byte at a time; joining long messages. No input or output |
| `src/core/link.h`, `link.c` | Start-up, sending with acknowledgement and retries, receiving, request-and-reply. All input, output and timing goes through a "port" |
| `host/port_posix.c` | The port for a USB-to-serial adapter on the modern Mac |
| `host/nokialink-host.c` | The command-line tool: `nokialink-host identify <device> [trace-file]` |
| `tests/` | The tests and the pretend phone; run with `tests/run-host.sh` |

The port for the Plus's serial chip is Phase 2. The core uses no
memory allocation and compiles for the 68000 to about 4 KB.

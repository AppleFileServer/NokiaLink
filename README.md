# NokiaLink

Software for the Macintosh Plus that talks to a Nokia 7110 directly over a
Nokia DLR-3P serial cable: contacts, calendar, text messages and whatever
else the phone's protocol offers. No other computer is involved.

Named MacNokia until 15 November 2026.

**Status (13 November 2026):** project started; no Plus code yet. The serial
cable is on order.

## How the phone talks

The 7110 speaks Nokia's FBUS protocol over the cable. The protocol is
described in the documentation of the open-source Gammu project
(`docs/manual/protocol/n7110.rst` and `nokia.rst`). NokiaLink is written
from that description. It contains no Gammu code: Gammu is licensed under
the GPL, and NokiaLink is MIT.

Starting a session over a DLR-3 cable:

1. Raise the serial control lines (the cable is powered from them).
2. At 19,200 baud send `AT`, `AT&F`, `AT*NOKIAFBUS`.
3. Switch to 115,200 baud, 8 data bits, no parity.
4. Send a run of `0x55` bytes to synchronise.

## Open questions

- **Speed.** The Plus's ROM serial driver stops at 57,600 baud. The serial
  chip's 3.672 MHz clock divided by 32 gives 114,750 baud (0.4% low), so
  the program must drive the chip itself. Untested.
- **Cable power.** The Plus has one output control line and no power pins.
  Whether that powers the DLR-3P is unknown.
- **Lower speeds.** Whether the phone accepts FBUS below 115,200 baud is
  untested.

## Testing with Gammu on a modern Mac

`gammurc-7110` holds Gammu settings for the phone on a USB-to-serial
adapter. Fill in the adapter's device name, then:

    gammu -c gammurc-7110 identify

Gammu's log goes to `traces/`, which is not tracked: it holds real
contacts and messages.

## Licence

MIT. See `LICENSE`.

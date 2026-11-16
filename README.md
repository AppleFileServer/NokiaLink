# NokiaLink

Software for the Macintosh Plus that talks to a Nokia 7110 directly over a
Nokia DLR-3P serial cable: contacts, calendar, text messages and whatever
else the phone's protocol offers. No other computer is involved.

Named MacNokia until 15 November 2026.

**Status (15 November 2026):** the FBUS link layer is written and
tested against a pretend phone on the modern Mac. It has not yet met
the real phone: the serial cable is on order. No Plus application yet.

- The specification (draft): [`docs/SPECIFICATION.md`](docs/SPECIFICATION.md)
- The plan: [`docs/PLAN.md`](docs/PLAN.md)
- The protocol as implemented: [`docs/FBUS.md`](docs/FBUS.md)
- The cable between the Plus and the Nokia cable, with the pinout:
  [`docs/CABLE.md`](docs/CABLE.md)

## What it is for

Runs on a Macintosh Plus with 4 MB of memory and System 6.0.7, linked to
the phone through the Plus's modem port. The owner's first wants:

- Keep contacts and the calendar in step between the Plus and the phone.
- Copy every text message to an archive on the Plus, then delete the
  copied messages from the phone.

## What the phone offers

Everything below is offered by the 7110 over the cable, according to the
Gammu project's documentation and its list of functions for this phone.
None of it has been tried on the owner's phone yet. The plan aims to
build all of it.

**Contacts**
- Read, write and delete entries, in the phone's memory and on the SIM.
- Each entry can hold several numbers, an e-mail address, a postal
  address, a note and a caller group.
- Read how many entries are used and free.
- Read the call lists: dialled, received and missed.
- Read speed dials.
- Read and set caller groups: name, ringtone and logo.

**Calendar**
- Read all notes, add a note, delete a note.
- Note types: meeting, call, birthday and reminder, each with an
  optional alarm and repeat.

**Text messages**
- List the folders (Inbox, Outbox, the user's own folders, templates).
- Read, save and delete messages in any folder.
- Send a message.
- Be told the moment a message arrives.
- Read and set the message centre number.
- Read picture messages.
- Receive cell broadcast messages.

**Calls**
- Dial a number, answer and hang up.
- Be told of an incoming call, with the caller's number.
- Send touch tones during a call.
- Set and cancel call diverts.
- See network service replies, such as a balance check.

**Phone status**
- Battery level and signal strength.
- Network name, cell and area codes.
- Model, firmware version and date, IMEI, hardware and product codes,
  month of manufacture.

**Clock**
- Read and set the date and time.
- Read and set the alarm.

**Sounds and pictures**
- Read and write ringtones.
- Play a tone on the phone's buzzer.
- Read and set the operator logo, start-up logo and welcome note.
- Read and set profiles (General, Silent and so on).

**WAP**
- Read, write and delete bookmarks.
- Read and write the connection settings.

**Remote control and service**
- Press the phone's keys from the computer.
- Restart the phone, or reset its settings to factory values.

Known in the wider phone family; whether they work on the 7110 is
uncertain:

- Turn on and read the hidden network monitor (engineering screens for
  cells, channels, battery and temperature).
- Read or replace the operator name the phone holds.
- Mirror the phone's screen text.
- Read the security code.

**Not possible over the cable on this phone**
- To-do lists, notes and files: the 7110 has none.
- Reading or writing the phone's firmware.
- Call audio.

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
  Whether that powers the DLR-3P is unknown. `docs/CABLE.md` says how it
  will be measured and what to do if it doesn't.
- **Lower speeds.** Whether the phone accepts FBUS below 115,200 baud is
  untested.

## Building and testing

On a modern Mac, with the command-line developer tools:

    tests/run-host.sh

This builds and runs the tests of the portable core (`src/core`), and
builds `build-host/nokialink-host`, a command-line tool that runs the
same core against the real phone through a USB-to-serial adapter:

    build-host/nokialink-host identify /dev/cu.usbserial-XXXX traces/identify.txt

The last argument is optional: a file to record every byte in.

## Testing with Gammu on a modern Mac

`gammurc-7110` holds Gammu settings for the phone on a USB-to-serial
adapter. Fill in the adapter's device name, then:

    gammu -c gammurc-7110 identify

Gammu's log goes to `traces/`, which is not tracked: it holds real
contacts and messages.

## Licence

MIT. See `LICENSE`.

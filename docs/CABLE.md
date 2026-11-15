# The cable between the Plus and the Nokia serial cable

How to join Nokia's DLR-3P serial cable to the Macintosh Plus's modem
port. Nothing here has been tried yet: every figure is marked **Doc**
(from documentation), **Inferred** or **To measure**.

## 1. The Plus's serial port

The modem port and the printer port are the same 8-pin mini-DIN socket
with the same signals. NokiaLink uses the **modem port**; the printer
port stays free for LocalTalk.

Looking into the socket on the back of the Plus (flat edge of the
socket's keying at the bottom):

        8   7   6
      5    4    3
         2    1

| Pin | Name | Direction | What it is |
|---:|---|---|---|
| 1 | HSKo | out | Handshake out. The serial chip's DTR line. The Plus's only control output |
| 2 | HSKi | in | Handshake in (CTS). On the Plus it also reaches the chip's clock input |
| 3 | TxD− | out | Transmit data. This is the line that matches RS-232 "transmit" |
| 4 | GND | | Signal ground |
| 5 | RxD− | in | Receive data. This is the line that matches RS-232 "receive" |
| 6 | TxD+ | out | Transmit data, inverted. Not used |
| 7 | (none) | | Not connected on a Plus |
| 8 | RxD+ | in | Receive data, reference. Tie to ground for RS-232 |

(**Doc**: Apple's published pinout for the Plus. Pin numbers are moulded
into most mini-DIN plugs; check them with a meter before soldering, as
drawings of this connector are often mirrored.)

The port has **no power pin**. Its outputs swing roughly ±4 to ±5 V
(**Doc**, RS-422/423 drivers), less than a PC's RS-232 port.

## 2. The Nokia cable's computer end

The DLR-3P ends in a 9-way D socket made for a PC's serial port. The
PC-side numbering:

| D-9 pin | RS-232 name | Direction (seen from the computer) |
|---:|---|---|
| 1 | DCD | in |
| 2 | RxD | in: data from the phone |
| 3 | TxD | out: data to the phone |
| 4 | DTR | out: also powers the cable |
| 5 | GND | |
| 6 | DSR | in |
| 7 | RTS | out: also powers the cable |
| 8 | CTS | in |
| 9 | RI | in |

The DLR-3P is an **active** cable: it holds a small converter that the
computer powers through DTR and RTS (**Doc**: Gammu raises both before
it talks). Which of pins 1, 6, 8 and 9 the cable really drives is
**To measure**.

## 3. The wiring

| Plus mini-DIN | to | D-9 (PC numbering) | Why |
|---:|---|---|---|
| 1 HSKo | → | 4 DTR **and** 7 RTS, joined | Power for the cable's converter |
| 3 TxD− | → | 3 TxD | Data to the phone |
| 5 RxD− | ← | 2 RxD | Data from the phone |
| 4 GND | — | 5 GND | Ground |
| 8 RxD+ | — | 5 GND | Makes the Plus's receiver read RS-232 |
| 2 HSKi | | not connected | NokiaLink doesn't use it |
| 6 TxD+ | | not connected | |
| 7 | | not connected | |

D-9 pins 1, 6, 8 and 9 are left unconnected.

(**Inferred**: this is the standard way a Macintosh is wired to an
RS-232 device, with DTR and RTS joined because the Plus has one control
output and the cable wants two.)

## 4. Two ways to build it

**A. An adapter (recommended).** A short lead with a mini-DIN-8 plug at
one end and a 9-way D *plug* (pins) at the other, wired as section 3.
The Nokia cable plugs into it unmodified, so the same cable still works
on the modern Mac with a USB adapter for the Gammu tests. The mini-DIN
end can come from any Macintosh serial cable cut in half; buzz out
which wire is which pin first, because printer and modem cables cross
some wires over.

**B. Modify the Nokia cable.** Cut off its D-9 socket and fit a
mini-DIN-8 plug, wired as section 3. Before cutting, buzz out which
wire goes to which D-9 pin. This loses the cable for use with a modern
computer.

An Apple-style "Macintosh to modem" cable (mini-DIN-8 to 25-way) with a
25-to-9-way adapter may already be wired this way. It is worth buzzing
one out if you have one: check that HSKo reaches **both** DTR and RTS.

## 5. The open question: power

A PC's port gives the cable about ±12 V on two lines. The Plus gives
about ±4 to ±5 V on one line, shared between both of the cable's power
pins. Whether that is enough is **unknown**.

**To measure** (plan, Phase 0, on the modern Mac before any soldering):

1. With the cable working under Gammu, measure the voltage on D-9 pins
   4 and 7 and the current each draws.
2. Disconnect pin 7 (RTS), then pin 4 (DTR), and see whether the link
   still works on one line.
3. Note the voltage on pin 2 (data from the phone) when idle.

If the Plus's one line isn't enough, the adapter gets its own supply:
a small 5 V to 9 V source feeding D-9 pins 4 and 7 through a diode
each, with its ground joined to pin 5. HSKo is then left unconnected.
The Plus's receiver accepts full RS-232 voltages (**Doc**), so a
stronger supply on the cable does it no harm.

## 6. Speed

The phone talks at 115,200 baud. The Plus's serial chip can make
114,750 baud (0.4% slow, well inside what serial links tolerate) by
taking its 3.672 MHz clock divided by 32 (**Inferred** from the chip's
data sheet and the Plus's clock; untested). No extra hardware is needed
for this: it is done in software, and it is the plan's Phase 2.

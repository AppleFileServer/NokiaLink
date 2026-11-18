# NokiaLink: plan

A plan for a Macintosh Plus application that does everything the Nokia
7110 offers over its serial cable (the list in `../README.md`).

- **Status:** 15 November 2026. The owner has answered the plan's
  questions (section 6). Nothing is built.
- **Machine:** Macintosh Plus, 4 MB, System 6.0.7 (programme decision P1).
- **Link:** the Plus's modem port (8-pin mini-DIN) to Nokia's DLR-3P
  cable through a short adapter. Wiring and pinout: `CABLE.md`.
- **Method:** as the other Plus applications in the programme: C with
  Retro68, logic in portable modules with tests on the modern Mac,
  results on the real Plus counted as the only proof
  (`../../Programme/ARCHITECTURE.md`).

Figures and claims are marked **Doc** (from documentation), **Inferred**
or **Untested** where it matters.

---

## 1. What makes this project hard

Three things decide whether it works at all. The plan settles them
first, before any of the application is written.

1. **Speed.** The phone talks at 115,200 baud. The Plus's ROM serial
   driver stops at 57,600. NokiaLink must program the serial chip
   itself to get 114,750 baud, and must catch each arriving byte within
   about a quarter of a millisecond or lose data (**Inferred**: the
   chip holds three bytes; a byte arrives every 87 millionths of a
   second).
2. **Cable power.** The Nokia cable is powered by the computer. The
   Plus offers one weak control line where a PC offers two strong ones.
3. **No emulator for the link.** Mini vMac has no serial port, so the
   link can only be tried on the real Plus. Everything above the link
   is therefore built and tested on the modern Mac first, against the
   real phone through a USB adapter.

## 2. Shape of the program

Four layers, each usable and testable without the ones above it.

| Layer | What it does | Where it is tested |
|---|---|---|
| **Serial link** | Sends and receives bytes on the modem port at 114,750 baud; controls the power line | Real Plus only |
| **FBUS** | Frames, checksums, acknowledgements, retries, splitting long messages, the start-up sequence | Modern Mac: recorded sessions, then the real phone |
| **Phone services** | One module per subject (contacts, calendar, messages and so on): builds requests, decodes replies | Modern Mac: recorded sessions, then the real phone |
| **Application** | Windows, menus, the Plus's own files, sync, archive | Mini vMac with a pretend phone, then the real Plus |

The FBUS and phone-service layers are plain C with no Macintosh calls.
The same source is compiled twice: into NokiaLink for the Plus, and
into a small command-line tool on the modern Mac (`nokialink-host`)
that talks to the real phone through a USB adapter. That tool is how
most of the protocol work gets proved without the Plus.

A **pretend phone** (a module that answers requests from a table of
recorded replies, with made-up contacts and messages) stands in for the
link under Mini vMac, so the windows and files can be developed and
shown there.

### 2.1 The serial link on the Plus

- NokiaLink drives channel A of the serial chip directly, not through
  the ROM driver. Start-up at 19,125 baud (the chip's nearest to
  19,200), then 114,750 baud: the chip's 3.672 MHz clock divided by 32
  (**Inferred**, **Untested**).
- Arriving bytes are taken by NokiaLink's own interrupt routine into a
  buffer. Only the modem port's receive entries in the system's
  interrupt table are replaced; the mouse, which shares the chip, is
  left alone. On quitting, the table and the chip are put back as
  found.
- If interrupts prove too slow at this speed, the fallback is the
  method LocalTalk uses: watch the chip in a tight loop, with
  interrupts held off, for the short time a reply is expected.
- The phone repeats anything that isn't acknowledged, and NokiaLink
  repeats anything the phone doesn't acknowledge, so an occasional lost
  byte costs time, not data.
- **Known risk:** floppy disk activity and LocalTalk both hold
  interrupts off for long stretches. NokiaLink avoids disk writes while
  a reply is arriving. Whether it can run with AppleTalk active on the
  printer port is to be measured (Phase 2).

### 2.2 The Plus's own files

Follows the programme's data rules (`ARCHITECTURE.md` §4).

**Changed on 16 November 2026 (decisions 7 and 8, programme P29 to
P31):** contacts, and in time the calendar and messages, are not
NokiaLink's own files. They live in the owner's one store of plain
text on the Plus, shared with other programs (the email program reads
the same contact records). The store's formats are set by programme P32; the rest of its layout
is not yet designed (programme Q24). Until it is, the list below describes what NokiaLink
needs from it, and "NokiaLink Data" holds only what belongs to
NokiaLink alone (phone settings, the record of the last sync).

- One folder, "NokiaLink Data", holding: Contacts, Calendar, Message
  Archive, Phone Settings (ringtones, logos, profiles, bookmarks), and
  the record of the last sync.
- Documented formats with a version number and checksums; fields
  written one by one, Mac Roman text; safe saves (new copy, verify,
  keep the previous).
- The message archive is a journal that is only ever added to.
- Plain-text export (tab-separated) of contacts, calendar and messages,
  readable in TeachText and a spreadsheet.
- A small independent reader on the modern Mac, so the data never
  depends on NokiaLink alone.
- These files hold personal data. Test files in the repository are
  made up; real ones are never committed.

### 2.3 Sync

The phone keeps no record of what changed. So:

- After each sync the Plus saves a copy of what the phone held.
- At the next sync it reads the phone again and compares three things:
  the phone now, the Plus now, and the saved copy.
- Changed on one side only: carried to the other side. Changed on both:
  NokiaLink shows both versions and the owner chooses.
- A calendar note can't be edited in place on the phone, so a change
  is sent as "delete, then add".
- Text the phone can't hold (long names, characters outside its
  alphabet) is shown before sending, never silently cut.
- Before the first sync, and before any restore, NokiaLink takes a full
  backup of the phone's contacts and calendar to a dated file.

### 2.4 Message archive and delete

- A message is deleted from the phone only after it has been written to
  the archive on disk and read back correctly.
- Every folder on the phone is read and copied to the archive: Inbox,
  sent messages, templates and the owner's own folders.
- Each archived message keeps its folder, sender or recipient, date and
  time where the phone gives one, and its text; picture messages keep
  their picture.
- An interrupted run loses nothing; the next run carries on. A message
  already in the archive is recognised and not stored twice.
- Only the Inbox and sent messages are ever cleared automatically.
  Other folders are cleared only when the owner asks, folder by folder.
- Clearing starts as a confirmed step with a count ("Delete 212
  archived messages from the phone?"). A setting makes it automatic for
  the Inbox and sent messages once the owner trusts it.

### 2.5 The application

A normal System 6 application: menu bar, movable windows, standard
dialogs, readable on the 512 × 342 screen.

| Window | Holds |
|---|---|
| Phone | Connection state, battery, signal, network, model and firmware; clock and alarm |
| Contacts | The list and an entry editor; Sync, Back Up, Restore; call lists; speed dials; caller groups |
| Calendar | Notes by date and an editor; Sync |
| Messages | Folders on the phone, the archive on the Plus, Archive and Delete, New Message |
| Calls | Dial, answer, hang up, touch tones; diverts; network service codes |
| Sounds & Pictures | Ringtones, logos, welcome note, profiles |
| WAP | Bookmarks and connection settings |
| Service | Key presses, restart, factory reset, and the uncertain engineering items |

Arrivals (a call, a message) show as a small notice while NokiaLink is
running. A desk accessory is specified and is built after B-1.0.0
(decision 18).

## 3. Phases

Each phase ends with something the owner can see or try, and a short
report. Version numbers follow the programme's rule (P22), starting at
B-0.1.0.

### Phase 0: prove the cable and phone on the modern Mac

Needs the cable and a USB-to-serial adapter. No Plus involved.

1. `gammu identify`; then read contacts, calendar and messages with
   Gammu, logging every byte to `traces/`. Read how many contacts the
   SIM and the phone's own memory each hold (section 7).
2. **Take a full Gammu backup of the phone** and keep it safe. Nothing
   in later phases writes to the phone before this exists.
3. Measure the cable's power needs (`CABLE.md` §5) and find which
   D-9 pins it uses.
4. Try the uncertain items (network monitor, operator name, screen
   mirror, security code) to learn which the 7110 answers.
   Try reading each of the further functions of `SPECIFICATION.md`
   §4.14 the same way (decision 17).
5. Try starting FBUS at lower speeds, to answer that open question.

**Result:** a yes or no on each open question, and recorded sessions to
build against.

### Phase 1: the protocol on the modern Mac

1. FBUS layer: framing, checksums, acknowledgements, retries, long
   messages, start-up sequence. Tests replay the Phase 0 recordings.
   **Written 15 November 2026, ahead of Phase 0,** from the protocol
   description (`FBUS.md`), with tests against a pretend phone. Still
   to do once Phase 0 has recordings: check it against them.
2. `nokialink-host identify` against the real phone.
3. Phone services, read-only first: status, clock, contacts, calendar,
   messages, call lists.

**Result:** the command-line tool reads everything from the real phone,
and its output matches Gammu's.

### Phase 2: the link on the real Plus

1. Build the adapter (`CABLE.md`).
2. A bare test program, "Link Test": powers the cable, starts FBUS,
   shows the phone's model and battery level.
3. Soak test: thousands of exchanges, counting lost bytes and retries;
   repeated with the mouse moving, with a floppy in use, and with
   AppleTalk active.
4. Choose interrupts or the tight-loop fallback from the figures.

**Result:** the Plus and the phone talking, with measured reliability.
**This is the point at which the project is known to be possible.** If
114,750 baud can't be made reliable, stop and reconsider (section 5).

### Phase 3: the application shell

Menus, the Phone window (status, clock, alarm), connecting and
disconnecting, the pretend phone under Mini vMac, the data folder and
file formats with their independent reader.

### Phase 4: contacts

Read into the Plus's file; back up and restore; the editor; write and
delete on the phone; two-way sync with the conflict screen; export.
Then call lists, speed dials and caller groups.

### Phase 5: calendar

The same sequence for calendar notes, with alarms and repeats.

### Phase 6: messages

Folder list; read every folder; the archive journal; archive with
verification; confirmed delete; automatic delete of the Inbox and sent
messages as a setting; export; send a
message; message centre number; picture messages; cell broadcast.

**After Phase 6 the owner's stated wants are all met.** B-0.9 or so;
a natural first release to live with.

### Phase 7: calls and live events

Notices of incoming calls and messages; dial from a contact; answer,
hang up, touch tones; diverts; network service codes.

### Phase 8: sounds and pictures

Ringtones (read, keep, send, play a tone); operator logo, start-up
logo, welcome note (with a simple 1-bit picture import from MacPaint
files); profiles.

### Phase 9: WAP

Bookmarks and connection settings: read, edit, keep copies, send.
Useful beside the MacWAP project.

### Phase 10: service

Key presses, restart, factory reset (behind a firm warning), and
whichever uncertain items Phase 0 showed to work.

### Phase 11: manual and release

User's Guide in the suite's manual style (7½ × 9 in, P24), release
checklist, B-1.0.0 after the owner's acceptance run on the Plus.

### Phase 12: the desk accessory

After B-1.0.0 (decision 18): the desk accessory of
`SPECIFICATION.md` §8.2, once the programme's shared desk-accessory
study is done.

## 4. Rules for the work

- **The phone is the owner's daily phone.** Read-only work comes first
  in every subject. Each new kind of write is tried once from the
  modern Mac, on a single test entry, before the Plus does it.
- **No Gammu code.** NokiaLink is written from the protocol
  descriptions. Gammu is used as a tool for comparison and backup.
- **Recordings and real data stay out of the repository.**
- **Emulator time is shared:** Mini vMac runs are kept short.
- **Memory is measured,** not assumed. Expected to be comfortable: the
  phone holds at most some hundreds of contacts, calendar notes and
  messages (the exact limits are read from the phone itself in
  Phase 0).

## 5. If the link can't be made reliable

In order of preference:

1. A lower FBUS speed, if Phase 0 finds the phone accepts one.
2. The tight-loop receive method (section 2.1).
3. A small speed-matching box between the Plus and the cable (a
   microcontroller that talks 115,200 to the phone and 57,600 to the
   Plus through the ROM driver). This adds hardware, which the project
   set out to avoid.

## 6. Decisions (the owner, 15 November 2026)

| # | Subject | Decision |
|---|---|---|
| 1 | Joining the Plus to the Nokia cable | An adapter lead, made by the owner (`CABLE.md`). The Nokia cable is not cut |
| 2 | Where contacts and calendar live on the Plus | NokiaLink's own files, with plain-text export. For now; may change if other suite applications want them. **Superseded by decision 7** |
| 3 | Messages | **Every** folder is read and copied to the archive. Only the Inbox and sent messages are cleared from the phone automatically; other folders are never cleared without being asked |
| 4 | Contacts | **The phone's own memory is to be used for contacts,** not the SIM, so that entries can hold more than a name and one number. Contacts still on the SIM are synced in the normal way until they are moved (see section 7) |
| 5 | Both sides changed the same entry | NokiaLink always asks: it shows both versions and the owner chooses. No standing rule |
| 6 | USB-to-serial adapter for the modern Mac | The owner is getting one. One with an FTDI chip, a 9-pin plug and true RS-232 levels is wanted (macOS has its driver built in) |
| 7 | Where contacts live (16 November 2026) | In the owner's one store of plain text on the Plus, which is the master (programme P29, P30). Contacts are vCards, one file per contact, in `Contacts/Main` and `Contacts/Archive` folders, with the dates as `X-LAST-USED` and `X-KEPT-ON` lines; Mac Roman text; checksums in one list per folder (programme P32). Nothing is pushed to iCloud; backups and the archive are the Pi's AppleShare drive's job |
| 8 | Main and Archive (16 November 2026) | Contacts are in two lists. NokiaLink syncs **Main** with the phone; Archive stays on the Plus. New contacts found on the phone are copied into Main. See section 8 |
| 9 | The 18-month check (16 November 2026) | On the 1st of each month, or the first time NokiaLink is opened after it, NokiaLink asks at startup about Main contacts not used for over 18 months. It applies to every contact alike. See section 8 |
| 10 | A contact deleted on the phone (16 November 2026) | It moves to Archive on the Plus; a sync never deletes it from the store. NokiaLink can show, search and edit the Archive list, and move contacts between the lists by hand |
| 11 | A new phone contact matching an archived one (16 November 2026) | The archived contact moves back to Main without asking; the fields the phone holds replace the stored ones |
| 12 | Call lists (16 November 2026) | Cleared from the phone after copying, as texts are |
| 14 | Codes are not contacts (18 November 2026) | Menu/USSD codes, carrier short-codes (balance, top-up, voicemail) and the like are never stored as contacts; sync skips them for Main. NokiaLink keeps a separate reference list of such codes instead. See SPEC CON-15 |
| 13 | SIM → phone memory (18 November 2026) | NokiaLink moves SIM data to the phone's own (far larger) memory as a standard step: copy each entry to phone memory, read it back to confirm, then delete the SIM original; a setting makes it automatic at sync. The "memory in use" setting cannot be changed over the cable (no FBUS command on the 7110), so NokiaLink prompts the owner to set it on the phone by hand. See SPEC CON-9/9a/9b |
| 13 | Calendar on the phone (16 November 2026) | As many notes and meetings on the phone as it can hold. How its limited memory is managed is left until the phone's limits are known (Phase 0) |
| 14 | Clocks (16 November 2026) | The Plus is the reference. Because its clock battery can go flat, the Plus's clock is used only when it passes a check; the owner confirms it on the first connection; within a small difference the phone is set from the Plus without asking; otherwise NokiaLink asks. NokiaLink never needs the internet for this. Rules: `SPECIFICATION.md` §4.13 |
| 15 | Printing (16 November 2026) | Included from the first version |
| 16 | Acceptance run (16 November 2026) | Thirty days of ordinary use, syncing at least weekly, one real monthly check and one restore (`SPECIFICATION.md` §12) |
| 17 | Further phone functions (16 November 2026) | The functions believed available but not in the first list (other number lists, display indicators, PIN state, IMSI, profile detail, message settings) go into the specification to be tried, and are removed if the phone refuses them (`SPECIFICATION.md` §4.14). Reaching the Pi gateway from the Plus through the phone while away is parked (section 9) |
| 18 | Desk accessory (16 November 2026) | A desk accessory is part of the specification (`SPECIFICATION.md` §8), built after the main program's first B-1.0.0 release |
| 19 | A new contact's date (16 November 2026) | A contact is given the date it is added as its last-used date, so the monthly check does not ask about it straight away |

## 7. The phone's two contact memories

The 7110 has two places for contacts (**Doc**: the protocol lists both,
as memory types 5 and 6):

- **The SIM card.** Each entry is a name and one number. The SIM sets
  how many entries there are and how long a name may be.
- **The phone's own memory.** Each entry can also hold further numbers,
  an e-mail address, a postal address and a note.

The phone has a setting for which memory it uses and shows. The owner's
phone appears to hold SIM contacts only, which suggests it is set to
the SIM (**Untested**: Phase 0 reads how many entries each memory
holds, which settles it).

What follows for NokiaLink:

- Both memories are read, backed up and synced. Each contact on the
  Plus records which memory it belongs to.
- A contact kept on the SIM can carry only a name and one number to the
  phone. If more is typed on the Plus, the extra stays on the Plus and
  NokiaLink says so; it is never silently dropped.
- Moving a contact from one memory to the other is offered as a
  deliberate action, not done by sync.
- **The owner's choice (15 November 2026): contacts are to live in the
  phone's own memory.** NokiaLink writes new contacts there, and offers
  to move the SIM's contacts across in one step (after a backup, with
  the SIM's copies removed only once the new ones are read back). The
  The owner set the phone's "memory in use" to the phone's own memory
  on 15 November 2026. Contacts already on the SIM stay there until
  they are moved.

## 8. Main, Archive and the 18-month check

Decided by the owner on 16 November 2026 (decisions 7 to 9; programme
P31).

### 8.1 The two lists

- Every contact is in **Main** or **Archive**. Sync (section 2.3)
  works on Main only: the phone holds Main, and nothing in Archive is
  sent to it.
- A contact added on the phone is copied into Main at the next sync.
- Moving a contact to Archive removes it from the phone at the next
  sync, after the usual backup. Moving it back sends it again.

### 8.2 The last-used date

- Each contact record carries a **last-used date** and a **kept-on
  date** (see 8.3).
- The phone keeps no such date (**to be checked** in Phase 1), so
  NokiaLink works it out. At every sync the texts and the call log are
  moved to the Plus (section 2.4; the phone has little room for
  either), and each number in them is matched against the contacts.
  A contact used since the last sync has its last-used date set to the
  date of the newest text or call.
- The phone's call lists are short. A call that has dropped off the
  list before a sync is never seen, so regular syncs matter.
- Other programs update the same date. The email program dates a
  contact when mail is sent to or received from it. It never creates a
  contact.
- A text or call from a number that matches an **archived** contact
  moves that contact back to Main, and the next sync sends it to the
  phone.

### 8.3 The monthly check

- When NokiaLink starts, it looks at whether a check is due: one is
  due on the 1st of each month, or on the first start after it. This
  relies on the Plus's clock being right.
- It lists the Main contacts whose last-used date **and** kept-on date
  are both more than 18 months old. It reads the stored dates only; it
  does not search the messages again.
- For each one (or all together) the owner chooses **Move to Archive**
  or **Stay in Main**.
- **Stay in Main** sets the kept-on date to today. The true last-used
  date is left alone. The contact is asked about again only when 18
  months have passed since the later of the two dates.
- The check treats every contact alike. Contacts reached only by
  WhatsApp will look unused to the phone until WhatsApp reaches the
  7110 (section 8.4); the owner answers "Stay in Main" for those.
- The owner's own card is never listed.

### 8.4 Not yet decided

- The first dates. The lists were drawn up on the modern Mac on 15 and
  16 November 2026 with a last-contact date for most contacts; those
  dates can start the last-used dates so that the first check doesn't
  list everyone. The lists themselves are private and are not in this
  repository.
- How the contacts now in iCloud reach the Plus (programme Q24).
- WhatsApp messages and email on the 7110: wanted later. Which piece
  of hardware does what, without the same message arriving twice, is
  not worked out.

## 9. The Plus reaching the Pi gateway while away: parked

**The owner's aim (16 November 2026):** with the Plus away from home
and the phone on its cable, connect to the Raspberry Pi gateway
(`../PiGateway`) through the phone. **Parked by the owner the same
day,** perhaps for a future 4G version of the 7110. Recorded here so
the findings are not lost.

**Route 1: a data call (the natural way).** Before NokiaLink starts
its own protocol, the 7110 answers ordinary modem commands on the same
cable. The Plus would dial the Pi's GSM board (the SIM800C of
`../MacWAP`) as a data call and talk to the gateway at up to 9,600
bits a second. No new hardware. It needs the network to carry 2G data
calls at both ends: Vodafone's network (which Lebara uses) keeps 2G
until spring 2030, but whether it still accepts data calls is not
published and must be tried. The owner may try it.

**Route 2: an ordinary voice call with the data sent as sound.**
Looked into and not recommended:

- The DLR-3P cable carries no call audio, and the Plus has no sound
  input. A modem box would have to be built between the Plus and the
  phone's headset connector, with a matching sound modem on the Pi.
- A mobile voice call does not carry sound as it is; it carries a
  description of speech, and ordinary modem tones do not survive.
  Purpose-made "data over voice" modems in the research literature
  reach about 1 to 2 thousand bits a second (the Hermes system: 1.2
  thousand, on real networks): roughly an eighth of a data call,
  around 150 characters a second, before any overhead.
- The call is charged by the minute while the link is up.

**Route 3: text messages as the carrier.** Works wherever the phone
has signal while 2G lasts, and MacWAP's message side is being built on
the same idea. Looked into on 16 November 2026; all figures are
estimates until measured:

- **Size:** a text carries 140 bytes; about 130 once each one is
  numbered so that a long item can be put back together.
- **Time:** a few seconds on the air for each text, and commonly 5 to
  10 seconds from one end to the other through the network's message
  centre. Texts are sent one after another, not side by side.
- **Speed:** so roughly 13 to 26 characters a second (100 to 200 bits
  a second). A data call is about 50 to 100 times faster; the voice
  call method of Route 2 about 6 to 10 times faster.
- **In practice:** a 500-character e-mail is 4 texts, 20 to 40 seconds.
  A 2,000-character page is 16 texts, about 1½ to 2½ minutes. Any
  request and its answer take at least 10 to 20 seconds.
- **The real limit is the daily allowance.** Lebara's fair-use limit
  on texts has been reported as 100 or 200 a day (not confirmed from
  Lebara's own current terms), and one user reports a 24-hour block
  for exceeding it. At 200 a day that is about 26,000 characters a day
  each way. The Pi's SIM has its own allowance.
- **Other points:** texts can arrive out of order, late or twice, so
  each needs a number and the far end must confirm; the 7110's small
  message memory must be emptied as texts arrive.

**Fit:** short messages, a mail header list, a few lines of chat. Not
browsing, and not files. **To measure** once the Pi's GSM board is
fitted: 20 texts each way, timed.

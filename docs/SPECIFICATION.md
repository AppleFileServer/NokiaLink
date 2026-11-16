# NokiaLink: Specification

- **Version of this specification:** 0.2 (draft), 16 November 2026
- **Status:** In development (`../../Programme/CATALOGUE.md`)
- **Group:** MacLife
- **Decision codes:** the numbered decisions in `PLAN.md` §6 (1 to 17 so
  far); programme decisions are P-codes.

This document says **what** NokiaLink does. `PLAN.md` says how and in
what order it is built; `FBUS.md` and `CABLE.md` describe the protocol
and the cable. Where they disagree, a later owner's decision wins and
the other document is corrected.

Requirements are numbered and meant to be testable. A requirement or
choice the owner has not yet agreed is marked **PROVISIONAL**. Facts
about the phone not yet seen on the owner's phone are marked **to be
checked**. A function the phone is believed to offer but which has not
been seen working is marked **TO BE TRIED**: it is attempted, and
removed from this specification if the phone refuses it (decision 17).
Section 15 lists what is still open.

## 1. Purpose and non-goals

NokiaLink is a Macintosh Plus application that talks to the owner's
Nokia 7110 directly over Nokia's DLR-3P serial cable. No other computer
takes part. It:

- keeps the owner's **Main** contacts in step between the Plus and the
  phone, and looks after the Main and Archive lists;
- moves text messages and the call log from the phone to the Plus at
  every sync, so the phone's small memory never fills;
- keeps the calendar in step;
- offers the rest of what the phone allows over the cable: status,
  clock, calls, ringtones and logos, WAP settings and service
  functions.

The Plus holds the master copy of everything NokiaLink reads or writes
(programme P30). The phone is a working copy of part of it.

**Non-goals**

- Nothing is sent to iCloud or any other outside service (P30).
- No backup or long-term archive of its own beyond the safety copies in
  section 6.5: backups and the archive belong to the Raspberry Pi's
  AppleShare drive (P30).
- No Gammu code (Gammu is GPL; NokiaLink is MIT).
- No to-do lists, notes, files, firmware reading or writing, or call
  audio: the 7110 does not offer them over the cable.
- No other phone models. The portable modules may suit other Nokia
  phones of the family, but none is tested or promised.
- WhatsApp messages and e-mail on the 7110 are wanted later and are not
  part of this version (section 14).

## 2. Target machines and System versions

| Configuration | Level (Programme README §6) | Evidence / note |
|---|---|---|
| Macintosh Plus, 4 MB, System 6.0.7, Finder | Supported once tested | The reference machine (P1) |
| Macintosh Plus, 4 MB, System 6.0.7, MultiFinder | Expected, untested | The link is time-critical; another program in the foreground may disturb it. To be measured |
| Macintosh Plus, 4 MB, AppleTalk active on the printer port | Unknown | LocalTalk holds interrupts off; measured in Phase 2. It matters because the Pi's AppleShare drive is reached this way |
| Macintosh Plus, 1 MB | Not planned | To be assessed after 1.0 (programme Q14) |
| 512 KB, earlier Systems, SE/30 and later | Unsupported | The serial routine is written for the Plus's chip and clock |

**Phone:** Nokia 7110, the owner's handset and firmware. **Cable:**
Nokia DLR-3P with the adapter lead of `CABLE.md`, on the modem port.

## 3. User workflows

### 3.1 Connect

1. The owner plugs the phone into the cable and opens NokiaLink.
2. NokiaLink powers the cable, starts the link and shows the Phone
   window: model, battery, signal and network. It compares the
   clocks and sets the phone's from the Plus's, or asks (4.13).
3. If the monthly check is due (3.4), it is shown before anything else.
4. NokiaLink offers to sync.

### 3.2 Sync

1. The owner chooses Sync (or accepts the offer at connection).
2. NokiaLink reads the phone's contacts, calendar, messages and call
   lists.
3. Texts and call records are written to the Plus, read back and
   checked; then the copied ones are cleared from the phone, texts
   and call lists alike (first
   with a count to confirm, later automatically if the owner turns
   that on).
4. Contacts and calendar notes changed on one side are carried to the
   other. Anything changed on both is shown, both versions side by
   side, and the owner chooses.
5. New contacts found on the phone are added to Main. A contact
   deleted on the phone moves to Archive.
6. Each contact used since the last sync has its last-used date
   updated. An archived contact that has been in touch moves back to
   Main and is sent to the phone.
7. A summary lists what was added, changed, moved and cleared.

### 3.3 Edit a contact

The owner opens a contact in the Contacts window, changes it and
saves. The change is written to the store at once and reaches the
phone at the next sync, or immediately if the phone is connected and
the owner chooses Send Now.

### 3.4 The monthly check

1. On the first start on or after the 1st of each month, NokiaLink
   lists the Main contacts not used for over 18 months.
2. For each, or for all together, the owner chooses **Move to Archive**
   or **Stay in Main**.
3. Archived contacts leave the phone at the next sync. Those kept are
   not asked about again for 18 months unless used.

### 3.5 Read and send a message

The owner opens the Messages window, reads messages on the phone or in
the archive on the Plus, and writes a new message to a contact or a
typed number. NokiaLink sends it through the phone.

### 3.6 Restore

If the phone is replaced or reset, the owner chooses Restore: NokiaLink
writes the Main contacts and the calendar back to the phone from the
store, after showing what will be written.

## 4. Functional requirements

### 4.1 Link (LNK)

- **LNK-1** NokiaLink drives channel A of the Plus's serial chip itself
  at 114,750 baud after a start-up at 19,125 baud, and controls the
  cable's power line (`PLAN.md` §2.1).
- **LNK-2** It follows the FBUS start-up sequence, framing, checksums,
  acknowledgements, retries and long-message handling of `FBUS.md`.
- **LNK-3** A lost or damaged frame is retried. After the retry limit
  the operation stops with a plain message; nothing half-done is left
  on the phone or the Plus (section 10).
- **LNK-4** On quitting, and on a crash-free exit of any kind, the
  serial chip and the interrupt table are put back as found. The mouse
  keeps working throughout.
- **LNK-5** NokiaLink does not write to disk while a reply is arriving.
- **LNK-6** Unplugging the phone at any moment is detected within a few
  seconds and reported; reconnecting needs no restart of the program.

### 4.2 Phone status and clock (PHN)

- **PHN-1** Show model, firmware version and date, IMEI, battery level,
  signal strength, and network name.
- **PHN-2** Read the phone's date, time and alarm; set them.
- **PHN-3** The clocks are managed by the rules of section 4.13.
- **PHN-4** Show how many contacts the SIM and the phone's own memory
  each hold and have free, and the same for messages where the phone
  reports it.

### 4.3 Contacts (CON)

- **CON-1** Read, add, change and delete contacts in the phone's own
  memory and on the SIM. New contacts are written to the phone's own
  memory (decision 4).
- **CON-2** A contact holds: name; several numbers, each with a kind
  (mobile, home, work, fax, general); e-mail addresses; a postal
  address; a note; a caller group; the list it is in (Main or
  Archive); the phone memory it belongs to; a last-used date and a
  kept-on date.
- **CON-3** The store may hold more than the phone can (more e-mail
  addresses, a longer name, characters outside the phone's alphabet).
  What will not fit is shown before sending and stays on the Plus; it
  is never silently cut or dropped (`PLAN.md` §2.3, §7).
- **CON-4** Sync works on **Main** only. Nothing in Archive is sent to
  the phone (decision 8).
- **CON-5** Sync is three-way: the phone now, the store now, and the
  copy saved at the last sync. A change on one side is carried across;
  a change on both is put to the owner, who chooses (decision 5).
- **CON-6** A contact found on the phone and not in the store is added
  to Main (decision 8). If its number matches an **archived** contact,
  that contact is moved back to Main without asking, and each field the
  phone holds replaces the stored one. Fields the phone does not hold
  are kept. The summary says so (decision 11).
- **CON-7** A contact deleted on the phone is moved to Archive in the
  store, without asking. It is never deleted from the store by a sync.
  The summary names it (decision 10).
- **CON-8** Moving a contact to Archive removes it from the phone at
  the next sync. Moving it to Main sends it at the next sync.
- **CON-9** Offer to move all SIM contacts to the phone's own memory in
  one step, after a backup and with a list shown first (`PLAN.md` §7).
- **CON-10** Read the call lists (dialled, received, missed), speed
  dials and caller groups; set speed dials and caller groups (name,
  ringtone, logo).
- **CON-11** Find a contact by typing part of a name or number.
- **CON-13** The Contacts window shows either list, Main or Archive,
  and the owner can switch between them, search both at once, open and
  edit an archived contact, and move a contact from one list to the
  other by hand (decision 10).
- **CON-14** Deleting a contact from the store altogether is a
  separate, confirmed command. It is never the result of a sync.
- **CON-12** NokiaLink never creates a contact from a message or call
  alone. Contacts come from the phone's own contact list or from the
  owner typing them.

### 4.4 Last-used dates and the 18-month check (USE)

- **USE-1** Each contact carries a last-used date and a kept-on date
  (decision 9, programme P31).
- **USE-2** At every sync, each number in the texts and call records
  brought from the phone is matched against the contacts in both
  lists. Numbers are compared in a way that treats `07…` and `+447…`
  as the same. A matched contact's last-used date becomes the date of
  its newest text or call, if that is later than the date it has.
- **USE-3** Sending a message or dialling a call from NokiaLink updates
  the contact's last-used date at once.
- **USE-4** A text or call matching an **archived** contact moves it to
  Main, and the summary says so.
- **USE-5** A check is due on the 1st of each month, or at the first
  start after it. It runs once per month.
- **USE-6** The check lists Main contacts for which both dates are more
  than 18 months before today. It reads the stored dates only.
- **USE-7** If the phone is connected when a check is due, NokiaLink
  syncs first, so that the dates are current. If it is not connected,
  the check still runs and says when the last sync was. **PROVISIONAL**
- **USE-8** **Stay in Main** sets the kept-on date to today and leaves
  the last-used date alone. **Move to Archive** moves the contact's
  file to the Archive list.
- **USE-9** The owner's own card is never listed.
- **USE-10** Other programs may update the last-used date in the store
  (the e-mail program does). NokiaLink accepts a later date it did not
  write.
- **USE-11** If the Plus's clock is not trusted (CLK-1), the check
  does not run, Stay in Main is not offered, and NokiaLink says why.

### 4.5 Calendar (CAL)

- **CAL-1** Read all notes; add and delete a note. A change is sent as
  delete-then-add, because the phone cannot edit in place.
- **CAL-2** Note kinds: meeting, call, birthday and reminder, each with
  an optional alarm and repeat.
- **CAL-3** Three-way sync as CON-5.
- **CAL-4** The owner wants as many calendar notes, meetings and the
  rest on the phone as it can hold. How the phone's limited memory is
  managed to make that most useful (which notes go when not all fit,
  and who decides) is **left open until the phone's limits are read**
  in Phase 0 (decision 13).

### 4.6 Messages (MSG)

- **MSG-1** List the phone's folders and read every message in each:
  Inbox, sent messages, templates and the owner's own folders
  (decision 3).
- **MSG-2** At every sync, copy every message not already in the
  archive on the Plus. Each keeps its folder, sender or recipient, date
  and time where the phone gives one, text, and picture if it has one.
- **MSG-3** A message is deleted from the phone only after it has been
  written to the archive and read back correctly.
- **MSG-4** Only the Inbox and sent messages are cleared
  automatically. Other folders are cleared only when the owner asks,
  folder by folder (decision 3).
- **MSG-5** Clearing begins as a confirmed step showing a count. A
  setting makes it automatic for the Inbox and sent messages.
- **MSG-6** An interrupted run loses nothing. A message already in the
  archive is recognised and not stored twice.
- **MSG-7** Write and send a message to a contact or a typed number;
  say how many parts a long message will be sent as; keep a copy in
  the archive.
- **MSG-8** Read and set the message centre number.
- **MSG-9** Show the archive on the Plus by person and by date; search
  it by text.
- **MSG-10** Read picture messages; receive cell broadcast messages.
- **MSG-11** Show a notice when a message arrives while NokiaLink is
  running.

### 4.7 Call log (LOG)

- **LOG-1** At every sync, copy the phone's dialled, received and
  missed lists to the call log on the Plus: number, name if matched,
  kind, and date and time where the phone gives one (**to be
  checked**: whether the 7110 reports a time for every list).
- **LOG-2** A record already copied is not copied again.
- **LOG-3** The phone's call lists are cleared after copying, as texts
  are: only after the records have been written to the Plus and read
  back correctly; first as a confirmed step showing a count, then
  automatically once the owner turns that on (decision 12).

### 4.8 Calls (CLL)

- **CLL-1** Dial a contact or a typed number; answer; hang up; send
  touch tones during a call.
- **CLL-2** Show a notice of an incoming call with the caller's number
  and name.
- **CLL-3** Read, set and cancel call diverts.
- **CLL-4** Send a network service code and show the reply (a balance
  check, for instance).

### 4.9 Sounds and pictures (SND)

- **SND-1** Read ringtones from the phone, keep them on the Plus, send
  one to the phone, and play a tone on the phone's buzzer.
- **SND-2** Read and set the operator logo, start-up logo and welcome
  note. A logo can be made from a MacPaint file.
- **SND-3** Read and set profiles.

### 4.10 WAP (WAP)

- **WAP-1** Read, add, change and delete bookmarks.
- **WAP-2** Read and write the connection settings, and keep named
  copies of them on the Plus (useful beside MacWAP).

### 4.11 Service (SVC)

- **SVC-1** Press the phone's keys from the Plus.
- **SVC-2** Restart the phone.
- **SVC-3** Reset the phone to factory settings, behind a firm warning
  and only after a full backup made in the same session.
- **SVC-4** The uncertain items (network monitor, operator name, screen
  mirror, security code) appear only if Phase 0 shows the owner's
  phone answers them.

### 4.12 The application (APP)

- **APP-1** A normal System 6 application: menu bar, movable windows,
  standard dialogs, readable on the 512 × 342 screen. Windows: Phone,
  Contacts, Calendar, Messages, Calls, Sounds & Pictures, WAP, Service
  (`PLAN.md` §2.5).
- **APP-2** Everything except live functions works with no phone
  connected: reading and editing contacts and the calendar, reading the
  archive and call log, the monthly check.
- **APP-3** Every write to the phone that removes or replaces something
  names what it will do before doing it.
- **APP-4** Before the first sync, and before any restore, a full
  backup of the phone's contacts and calendar is taken to a dated file.
- **APP-5** Each sync ends with a summary, which is also added to a
  sync log on the Plus.
- **APP-6** Standard Edit menu, and Undo for edits not yet saved.
- **APP-7** Printing, from the first version (decision 15): the
  contact list (Main, Archive or both), a single contact, a message
  thread, the call log for a chosen period, and the calendar for a
  chosen period. Standard Page Setup and Print dialogs; ImageWriter
  and LaserWriter.

### 4.14 Further phone functions, to be tried (EXT)

Each of these is believed to be offered by the 7110 over the cable but
is not in the project's first list. Each is **TO BE TRIED**: read-only
from the modern Mac first (Phase 0 and Phase 1), then built if it
works, and struck out here if it does not (decision 17).

- **EXT-1** Read the phone's other number lists: the owner's own
  number, the voicemail number, and the SIM's fixed-dialling list.
  Set the own number and voicemail number.
- **EXT-2** Show the phone's display indicators in the Phone window:
  unread message, call in progress, keypad locked, message memory
  full.
- **EXT-3** Show whether the phone is waiting for a PIN or PUK, and
  let the owner type the PIN on the Plus. The PIN is never stored.
- **EXT-4** Show the SIM's identity number (IMSI) in the Phone window.
- **EXT-5** Read and set each profile's detail: ringtone, volume,
  alert type, keypad tones, warning tones and message tone (extends
  SND-3).
- **EXT-6** Read and set the message settings: validity period and
  delivery reports; save a message into a folder on the phone, as a
  template for instance (extends MSG-8).

### 4.13 Clocks (CLK)

Three clocks are involved: the Plus's, the phone's, and the network's
(seen only in the time stamps the network puts on arriving texts). The
Plus is the reference (decision 14), but its clock is kept by a
battery that can go flat, after which it may show any date. So the
Plus's clock is used only once it has passed two tests.

- **CLK-1 The floor.** NokiaLink keeps the latest moment it knows has
  passed: the later of the last sync and the newest network time stamp
  on any archived text. A Plus clock earlier than that floor, or
  earlier than 2026, is **not trusted**. A flat battery almost always
  shows such a date.
- **CLK-2 The first connection.** The first time NokiaLink meets the
  phone, it shows the Plus's date and time and asks the owner to
  confirm they are right (or correct them) before anything is set or
  stamped. Then the phone is set from the Plus.
- **CLK-3 Each later connection, Plus trusted:**
  - the two clocks are within **five minutes** (**PROVISIONAL**): the
    phone is set from the Plus, without asking, and the sync log
    records by how much;
  - they differ by more: NokiaLink shows both and asks. "The Plus is
    right" sets the phone. "The phone is right" sets the Plus. The
    owner can also type the time. A difference of almost exactly one
    hour is pointed out as probably the clocks going forward or back.
- **CLK-4 Each later connection, Plus not trusted:** the phone is
  **never** set from the Plus. NokiaLink says the Plus's clock looks
  wrong (naming the likely cause, the clock battery), shows the
  phone's time and asks whether to set the Plus from it, or to type
  the time.
- **CLK-5 While the Plus's clock is not trusted,** nothing is stamped
  with today's date: the monthly check does not run and Stay in Main
  is not offered (USE-11). Syncing still works, because the dates on
  texts and calls come from the network and the phone, not from the
  Plus.
- **CLK-6 The Plus's clock set from the internet.** When the Plus is
  on the network, its clock can be set from the Raspberry Pi, which
  takes the time from the internet. That is the Pi gateway's job, not
  NokiaLink's, and NokiaLink never needs it: the rules above work with
  no network at all. A Plus clock set that way simply passes CLK-1.
  (To be raised in `../PiGateway`.)
- **CLK-7** The phone's alarm is left as it is when the phone's clock
  is set.
- **CLK-8** Every clock change NokiaLink makes, to either machine, is
  written in the sync log with the old and new times.

## 5. Non-functional requirements

- **Responsiveness:** the pointer and menus stay live during a sync;
  a progress display names the step and can be cancelled between
  steps. Cancelling leaves both sides consistent.
- **Duration:** a routine sync of about 60 contacts, the calendar and a
  week's messages should take under two minutes. **PROVISIONAL**, to be
  replaced by measured figures.
- **Start-up:** the Contacts window is usable within about ten seconds
  of opening from a hard disk, phone connected or not. **PROVISIONAL**
- **Interruptions:** a power cut, crash or unplugged cable at any point
  loses no message and damages no file (section 10).
- **Text:** Chicago and Geneva at their built-in sizes; no text smaller
  than 9 point.
- **Disk:** the message archive only grows. NokiaLink shows its size
  and warns when the disk is nearly full.

## 6. Data model and file formats

NokiaLink owns very little. Contacts, the calendar, messages and the
call log live in the owner's one store on the Plus and are shared with
other programs (programme P29 to P32, decision 7).

### 6.1 In the one store

| Information | Form (P32) |
|---|---|
| Contacts | vCard, one file per contact, in `Contacts/Main` and `Contacts/Archive`. Moving between lists is moving the file |
| Calendar | vCalendar, one file per note |
| Text messages | Tab-separated lines, one per message, only ever added to. A picture message's picture is a separate file named in its line |
| Call log | Tab-separated lines, one per call, only ever added to |

- Text is Mac Roman with CR line ends. Characters outside Mac Roman
  cannot be stored and are written in Latin letters.
- A "Checksums" list in each folder holds a checksum for every file in
  it. NokiaLink updates the entry for any file it writes and checks the
  entry before trusting a file it reads.
- vCard and vCalendar version 2.1 is the working assumption (the 7110
  uses it). **PROVISIONAL** (programme Q24)
- The store's name and place on disk, the file-naming rule, the exact
  columns of the two tab-separated files, and how two programs open the
  same file safely are not yet designed (programme Q24, Q5). NokiaLink
  needs a stable identity for each contact that survives a change of
  name; an `X-` line holding one is **PROVISIONAL**.

### 6.2 Extra lines NokiaLink uses in a contact

| Line | Holds |
|---|---|
| `X-LAST-USED` | Date of the newest text, call or e-mail, as `YYYYMMDD` |
| `X-KEPT-ON` | Date the owner last chose Stay in Main; empty if never |
| `X-NOKIA-MEMORY` | `PHONE` or `SIM`: which of the phone's memories the contact belongs to. **PROVISIONAL** |
| `X-NOKIA-GROUP` | The caller group. **PROVISIONAL** |

Lines NokiaLink does not know are kept and written back unchanged.

### 6.3 NokiaLink's own folder, "NokiaLink Data"

- **Last Sync:** what the phone held at the end of the last sync, for
  the three-way comparison, with the date and time.
- **Phone Settings:** ringtones, logos, profiles, WAP bookmarks and
  connection settings kept from the phone.
- **Sync Log:** the summaries of past syncs, as text.
- **Preferences:** automatic clearing on or off, the date of the last
  monthly check, window positions.

These follow `../../Programme/ARCHITECTURE.md` §4: documented formats
with a version number, checksums, safe saves, and an independent
reader on the modern Mac.

### 6.4 Dates

Dates in the store are written out in full (`YYYYMMDD`, with a time
where there is one), never as a count from 1904. The phone's times are
local time with no zone; they are stored as given.

### 6.5 Safety copies

- Before the first sync and before any restore: a dated full backup of
  the phone's contacts and calendar (APP-4).
- Every save writes a new copy, verifies it, and keeps the previous
  version until the next save succeeds.

## 7. Shared dependencies and integration

- **The one store** (P29 to P32): shared with the e-mail program
  (a patched Eudora or the owner's own), which reads contacts and
  updates `X-LAST-USED` but never creates a contact (P31).
- **The Raspberry Pi's AppleShare drive** (`../PiGateway`): takes
  backups and the archive of the store (P30). NokiaLink does not talk
  to it; whether the two can be active at once depends on the AppleTalk
  measurement in Phase 2.
- **MacWAP** (`../MacWAP`): NokiaLink can keep and send the phone's WAP
  settings for it. No other link between the two.
- **MacCalendar** (proposed) and the calenDAr desk accessory: both
  would use the same calendar files. Nothing is assumed about
  calenDAr's format until it is investigated (P15, Q1).
- **MacPaint files** as the source of logos (P12).
- **Hardware:** Nokia DLR-3P cable; the adapter lead of `CABLE.md`.
- **Creator and types:** creator `NkLk` **PROVISIONAL**; its own files
  use types beginning `Nk`. Files in the store are type `TEXT` so that
  any program opens them. To be added to
  `../../Programme/ARCHITECTURE.md` §6 when agreed.

## 8. Desk Accessory suitability assessment

- Frequent, short task? Partly: looking up a number is; sync is not.
- Useful while another application is in use? A contact lookup would
  be. The link itself cannot share the machine safely with a
  foreground program.
- Small memory and window? A lookup, yes.
- Interruption-tolerant? A read-only lookup, yes. Anything using the
  link, no.
- Simple, safe data access? Reading one vCard file is simple and safe.
- **Conclusion:** no DA in this version. A read-only **contact lookup
  DA** over the store is worth assessing later, after the programme's
  shared DA study (P5). It would not use the phone.

## 9. Memory and performance constraints

- Memory is measured, not assumed (`PLAN.md` §4). A starting partition
  of 512 KB is **PROVISIONAL** until Phase 3 measures real use.
- Sizes to design for: up to 1,000 contacts in the store, the phone's
  own limits for contacts, notes and messages as read in Phase 0, and a
  message archive that grows without limit (it is read in parts, never
  loaded whole).
- At 114,750 baud a byte arrives every 87 millionths of a second and
  the chip holds three. The receive routine must take each byte within
  about a quarter of a millisecond. Phase 2's soak test sets the
  accepted retry rate.

## 10. Error handling and recovery

| Event | What happens |
|---|---|
| Cable unplugged or phone off during a sync | The step in progress is abandoned. Messages already archived and verified stay archived; none is deleted from the phone unless verified. The next sync carries on |
| Reply damaged or missing | Retried; then the operation stops with a plain message naming the step |
| Disk full or locked | Nothing is deleted from the phone. The owner is told how much room is needed |
| A file in the store fails its checksum | It is reported and **never written over**. Sync leaves that record alone and continues with the rest |
| A vCard another program wrote that NokiaLink cannot fully read | Unknown lines are kept. If the card cannot be read at all it is treated as damaged |
| The phone's clock unset (its battery was out) | Treated as a large difference: the owner is asked, and "The Plus is right" sets it (CLK-3) |
| Two contacts on the phone with the same number | Both are kept; the summary points them out. Nothing is merged without asking |
| Interrupted save | The previous version is still there and is used; the half-written copy is discarded |
| Out of memory | The operation stops cleanly before changing anything on the phone |
| Plus clock wrong (flat clock battery) | The phone is not set from it; the monthly check waits; the owner is offered the phone's time (CLK-4, CLK-5) |
| Phone full when sending a contact or note | The owner is told which entries did not go; they stay marked as waiting |

## 11. Security and privacy

- The store holds names, numbers, addresses and the full text of the
  owner's messages. By P29 it is not encrypted, so that any program can
  read it. Anyone with the Plus's disk, or the Pi's backup, can read it.
- Nothing leaves the Plus through NokiaLink except what it sends to the
  phone.
- Real contacts, messages and recordings of the phone are never
  committed to the repository. Test data is made up.
- The security code function (SVC-4), if the phone answers it, shows
  the code only on request and never writes it to disk.
- Factory reset and "clear folder" are the destructive functions; each
  needs an explicit confirmation naming what will be lost.

## 12. Testing and acceptance criteria

- **Host tests** (`tests/run-host.sh`): the FBUS layer and each phone
  service against the pretend phone and against recordings of the real
  one; the sync comparison; number matching; the 18-month rule with
  dates either side of the boundary, month ends and leap years; the
  clock rules with a Plus clock before the floor, in 1904, and one
  hour out; vCard
  reading and writing, including cards written by other programs.
- **Against the real phone from the modern Mac** (`nokialink-host`):
  every read, with output compared to Gammu's; every kind of write
  tried once on a single test entry before the Plus does it.
- **Emulator:** the application's windows, files and monthly check
  under Mini vMac with the pretend phone. Runs kept short.
- **On the real Plus:** the link soak test (Phase 2), then a written
  checklist covering every workflow in section 3 and every row of
  section 10.

**Criteria for B-1.0.0**

1. Every requirement in section 4 not marked deferred passes on the
   real Plus with the owner's phone.
2. Thirty days of the owner's ordinary use, syncing at least weekly,
   with no lost or duplicated message and no contact changed without
   the owner's choice.
3. One monthly check carried out for real.
4. A restore to the phone from the store, checked entry by entry.
5. The independent reader on the modern Mac reads every file NokiaLink
   wrote.

## 13. Documentation and release artefacts

User's Guide in the suite's manual style (7½ × 9 in, P24); Read Me;
release disk image and MacBinary; source archive; `SHA256SUMS`; the
adapter lead's build notes (`CABLE.md`); an environment archive entry.
Checklists: `../../Programme/templates/RELEASE-CHECKLIST.md` and
`ARCHIVE-CHECKLIST.md`.

## 14. Deferred features and future possibilities

- **Sending things to other phones by text** ("smart messages"): a
  contact card, a calendar note, a ringtone, a logo, a picture message
  or a WAP bookmark, sent as a specially marked text to another
  compatible Nokia. NokiaLink would compose these itself; deferred
  until ordinary sending is proved.
- **The Plus reaching the Pi gateway while away, through the phone.**
  The owner's aim; parked on 16 November 2026, perhaps for a future 4G
  version of the 7110. The natural route is a data call, with the
  phone acting as the Plus's modem on the same cable; sending the data
  as sound over a voice call was looked into and is not recommended.
  Findings in `PLAN.md` §9. NokiaLink need only leave the cable in a
  state other programs can use.
- **WhatsApp messages and e-mail on the 7110.** Wanted by the owner
  (WhatsApp within about six months). Which piece of hardware does
  what, without the same message arriving twice, is not worked out.
  Until then, contacts reached only by WhatsApp look unused to the
  phone and the owner answers Stay in Main.
- **A contact lookup desk accessory** (section 8).
- **Other programs writing last-used dates** beyond the e-mail program.
- **A 1 MB Plus** and other machines.
- **A speed-matching box**, only if the link cannot be made reliable
  (`PLAN.md` §5).

## 15. Open questions

Answered by the owner on 16 November 2026 and now in the text above:
a contact deleted on the phone (decision 10); a new phone contact
matching an archived one (decision 11); the call lists (decision 12);
the clocks (decision 14); printing (decision 15); the acceptance run
(decision 16).

Still open, for the owner:

1. **Calendar notes on the phone** (CAL-4): waits on the phone's
   limits (decision 13).
2. **Five minutes** as the difference below which the phone's clock is
   set without asking (CLK-3).
3. Creator code `NkLk`; a starting partition of 512 KB; no desk
   accessory in this version.

To be checked on the phone (Phase 0 and Phase 1): each function in
section 4.14; whether the phone
keeps any last-used date itself; whether every call list carries a
time; whether the call lists can be cleared over the cable; the limits
on contacts, notes and messages; which uncertain service items it
answers; whether it accepts a lower speed.

Waiting on the programme: the store's layout (Q24); safe shared file
access (Q5); running with AppleTalk active (Phase 2); setting the
Plus's clock from the Pi (`../PiGateway`).

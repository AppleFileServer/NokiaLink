/* NokiaLink's resources. */

#include "Menus.r"
#include "Dialogs.r"
#include "Windows.r"
#include "Processes.r"

/* Replaces Retro68's default SIZE resource; only MultiFinder reads it
   (under the Finder alone NokiaLink has the whole heap). 512 KB is
   PROVISIONAL until memory is measured (SPECIFICATION.md section 9).
   Not able to run in the background: the link can't share the machine
   (section 2). */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    cannotBackground,
    needsActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    notHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    512 * 1024,
    384 * 1024
};

/* The enable flags give item n as bit n-1: 0x7FFFFFFD leaves the
   divider (2) off. Desk accessories are added after it at start-up. */
resource 'MENU' (128) {
    128, textMenuProc;
    0x7FFFFFFD, enabled;
    apple;
    {
        "About NokiaLink\0xC9", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (129) {
    129, textMenuProc;
    allEnabled, enabled;
    "File";
    {
        "Quit", noIcon, "Q", noMark, plain;
    }
};

/* For desk accessories; NokiaLink's own windows don't use it yet.
   0x7FFFFFFD leaves the divider (2) off. */
resource 'MENU' (130) {
    130, textMenuProc;
    0x7FFFFFFD, enabled;
    "Edit";
    {
        "Undo", noIcon, "Z", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Cut", noIcon, "X", noMark, plain;
        "Copy", noIcon, "C", noMark, plain;
        "Paste", noIcon, "V", noMark, plain;
        "Clear", noIcon, noKey, noMark, plain;
    }
};

/* The View menu: windows the owner can open. */
resource 'MENU' (131) {
    131, textMenuProc;
    allEnabled, enabled;
    "View";
    {
        "Phone", noIcon, noKey, noMark, plain;
        "Contacts", noIcon, noKey, noMark, plain;
        "Messages", noIcon, noKey, noMark, plain;
        "Calendar", noIcon, noKey, noMark, plain;
        "Calls", noIcon, noKey, noMark, plain;
    }
};

/* The Actions menu: the things NokiaLink does to the phone. The divider
   (item 3) is off: 0x7FFFFFFB clears bit 2. */
resource 'MENU' (132) {
    132, textMenuProc;
    0x7FFFFFFB, enabled;
    "Actions";
    {
        "New Message\0xC9", noIcon, "N", noMark, plain;
        "Add Appointment\0xC9", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Sync Contacts\0xC9", noIcon, noKey, noMark, plain;
        "Migrate SIM to Phone\0xC9", noIcon, noKey, noMark, plain;
        "Set Clock\0xC9", noIcon, noKey, noMark, plain;
    }
};

resource 'MBAR' (128) {
    { 128, 129, 130, 131, 132 }
};

/* The one NokiaLink window. Its content and title switch between views
   (Phone, Contacts, Messages) from the View menu. */
resource 'WIND' (128) {
    {40, 20, 300, 340},
    noGrowDocProc, invisible, noGoAway, 0, "NokiaLink",
    noAutoCenter
};

/* About NokiaLink: ^0 is the version. */
resource 'ALRT' (128) {
    {60, 96, 200, 416}, 128,
    {
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1
    },
    noAutoCenter
};

resource 'DITL' (128) {
    {
        {108, 236, 128, 304}, Button { enabled, "OK" };
        {12, 16, 30, 304}, StaticText { disabled, "NokiaLink ^0" };
        {36, 16, 72, 304}, StaticText { disabled,
            "The Macintosh Plus and the Nokia 7110, on a serial cable." };
        {80, 16, 98, 304}, StaticText { disabled,
            "\0xA9 2026 Jack McAteer" };
    }
};

/* New Message: a recipient and the text. */
resource 'DLOG' (200) {
    {80, 60, 250, 420}, dBoxProc, invisible, noGoAway, 0, 200, "New Message",
    noAutoCenter
};

resource 'DITL' (200) {
    {
        {138, 288, 158, 348}, Button { enabled, "Send" };
        {138, 208, 158, 268}, Button { enabled, "Cancel" };
        {14, 14, 30, 50}, StaticText { disabled, "To:" };
        {12, 54, 30, 348}, EditText { enabled, "" };
        {40, 14, 56, 90}, StaticText { disabled, "Message:" };
        {60, 14, 128, 348}, EditText { enabled, "" };
    }
};

/* Add Appointment: a meeting or a reminder, with date, time and text. */
resource 'DLOG' (201) {
    {70, 50, 280, 420}, dBoxProc, invisible, noGoAway, 0, 201, "Add Appointment",
    noAutoCenter
};

resource 'DITL' (201) {
    {
        {178, 298, 198, 358}, Button { enabled, "OK" };
        {178, 218, 198, 278}, Button { enabled, "Cancel" };
        {14, 14, 30, 60}, StaticText { disabled, "Type:" };
        {12, 70, 30, 180}, RadioButton { enabled, "Meeting" };
        {12, 190, 30, 310}, RadioButton { enabled, "Reminder" };
        {44, 14, 60, 60}, StaticText { disabled, "Date:" };
        {42, 70, 60, 230}, EditText { enabled, "" };
        {72, 14, 88, 60}, StaticText { disabled, "Time:" };
        {70, 70, 88, 160}, EditText { enabled, "" };
        {100, 14, 116, 60}, StaticText { disabled, "Text:" };
        {118, 14, 170, 358}, EditText { enabled, "" };
    }
};

/* Confirmation (OK/Cancel): ^0 is the message. */
resource 'ALRT' (202) {
    {90, 100, 210, 420}, 202,
    {
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1
    },
    noAutoCenter
};

resource 'DITL' (202) {
    {
        {88, 240, 108, 300}, Button { enabled, "OK" };
        {88, 160, 108, 220}, Button { enabled, "Cancel" };
        {12, 16, 80, 300}, StaticText { disabled, "^0" };
    }
};

/* Notice (one button): ^0 is the message. */
resource 'ALRT' (203) {
    {90, 100, 210, 420}, 203,
    {
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1;
        OK, visible, sound1
    },
    noAutoCenter
};

resource 'DITL' (203) {
    {
        {88, 240, 108, 300}, Button { enabled, "OK" };
        {12, 16, 80, 300}, StaticText { disabled, "^0" };
    }
};

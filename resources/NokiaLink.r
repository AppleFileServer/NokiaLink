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
    }
};

resource 'MBAR' (128) {
    { 128, 129, 130, 131 }
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

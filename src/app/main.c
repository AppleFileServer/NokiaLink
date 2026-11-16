/* NokiaLink: the Macintosh Plus and the Nokia 7110 on a serial cable
   (docs/SPECIFICATION.md). The application's shell: the menus, the About
   box, desk accessories, the Phone window and the event loop. The link
   to the phone is not in it yet (docs/PLAN.md, Phases 2 and 3). */

#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Devices.h>
#include <ToolUtils.h>
#include <OSUtils.h>
#include <Memory.h>

#include "version.h"

enum {
    kMenuBar = 128,
    kAppleMenu = 128, kAboutItem = 1,
    kFileMenu = 129, kQuitItem = 1,
    kEditMenu = 130,
    kPhoneWindow = 128,
    kAboutAlert = 128
};

/* The stack: the Toolbox's own calls and interrupts come on top of what
   NokiaLink uses. Set before the heap is first grown. */
enum { kStackSize = 16L * 1024 };

static Boolean gQuit;
static WindowPtr gPhoneWindow;

static void About(void)
{
    ParamText(kAppVersionP, "\p", "\p", "\p");
    Alert(kAboutAlert, nil);
}

static void DrawPhoneWindow(void)
{
    GrafPtr saved;

    GetPort(&saved);
    SetPort(gPhoneWindow);
    EraseRect(&gPhoneWindow->portRect);
    TextFont(0);                /* the system font: Chicago 12 */
    TextSize(0);
    MoveTo(16, 32);
    DrawString("\pNo phone connected.");
    TextFont(3);                /* Geneva */
    TextSize(9);
    MoveTo(16, 56);
    DrawString("\pThe link to the phone is not built yet.");
    TextFont(0);
    TextSize(0);
    SetPort(saved);
}

static void DoMenu(long choice)
{
    short menu = HiWord(choice), item = LoWord(choice);
    Str255 name;

    switch (menu)
    {
    case kAppleMenu:
        if (item == kAboutItem)
            About();
        else
        {
            GetMenuItemText(GetMenuHandle(kAppleMenu), item, name);
            OpenDeskAcc(name);
        }
        break;
    case kFileMenu:
        if (item == kQuitItem)
            gQuit = true;
        break;
    case kEditMenu:
        SystemEdit(item - 1);   /* a desk accessory's, if one is in front */
        break;
    }
    HiliteMenu(0);
}

static void HandleEvent(EventRecord *event)
{
    WindowPtr window;

    switch (event->what)
    {
    case mouseDown:
        switch (FindWindow(event->where, &window))
        {
        case inMenuBar:
            DoMenu(MenuSelect(event->where));
            break;
        case inSysWindow:
            SystemClick(event, window);
            break;
        case inDrag:
            DragWindow(window, event->where, &qd.screenBits.bounds);
            break;
        case inContent:
            if (window != FrontWindow())
                SelectWindow(window);
            break;
        }
        break;
    case keyDown:
    case autoKey:
        if (event->modifiers & cmdKey)
            DoMenu(MenuKey((char)(event->message & charCodeMask)));
        break;
    case updateEvt:
        window = (WindowPtr)event->message;
        BeginUpdate(window);
        if (window == gPhoneWindow)
            DrawPhoneWindow();
        EndUpdate(window);
        break;
    }
}

static void Init(void)
{
    if (LMGetCurStackBase() - kStackSize < LMGetApplLimit())
        SetApplLimit((Ptr)(LMGetCurStackBase() - kStackSize));
    MaxApplZone();
    MoreMasters();
    MoreMasters();
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(nil);
    FlushEvents(everyEvent, 0);
    InitCursor();

    SetMenuBar(GetNewMBar(kMenuBar));
    AppendResMenu(GetMenuHandle(kAppleMenu), 'DRVR');
    DrawMenuBar();

    gPhoneWindow = GetNewWindow(kPhoneWindow, nil, (WindowPtr)-1L);
}

int main(void)
{
    EventRecord event;

    Init();
    if (gPhoneWindow)
        ShowWindow(gPhoneWindow);
    while (!gQuit)
    {
        if (WaitNextEvent(everyEvent, &event, 30, nil))
            HandleEvent(&event);
    }
    return 0;
}

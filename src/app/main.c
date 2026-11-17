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

#include <string.h>

#include "version.h"
#include "nlinfo.h"
#ifdef NOKIALINK_SMOKE
#include "smoke.h"
#endif

/* The status shown in the Phone window. Until the serial link is built,
 * it is filled from canned replies decoded by the nlinfo module, so the
 * window shows the decode path working and is marked as sample data. */
static char gModel[32], gFirmware[48], gBattery[48], gNetwork[80], gClock[48];

static void append_num(char *s, long v)
{
    char t[12]; int n = 0, i; if (v == 0) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    i = (int)strlen(s);
    while (n) s[i++] = t[--n];
    s[i] = 0;
}

static void BuildSampleStatus(void)
{
    nl_info_ident id; int pct; nl_info_network net; nl_stamp clk;
    static const char *mons[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                                 "Jul","Aug","Sep","Oct","Nov","Dec"};
    nl_u8 b[64]; long n; const char *t = "V 04.80\r23-02-00\rNSE-5\r"; int i;

    /* identify (0xD2 text) */
    memset(b,0,sizeof b); b[3]=0xD2; for(n=4,i=0;t[i];i++) b[n++]=(nl_u8)t[i];
    gModel[0]=0; gFirmware[0]=0;
    if (nl_info_parse_identify(b,n,&id)==NL_INFO_OK) {
        strcpy(gModel, "Nokia 7110 ("); strcat(gModel, id.model); strcat(gModel, ")");
        strcpy(gFirmware, "Firmware "); strcat(gFirmware, id.version);
        strcat(gFirmware, ", "); strcat(gFirmware, id.date);
    }
    /* battery (body[5]) */
    memset(b,0,sizeof b); b[3]=0x03; b[5]=77;
    strcpy(gBattery, "Battery ");
    if (nl_info_parse_battery(b,6,&pct)==NL_INFO_OK) { append_num(gBattery, pct); strcat(gBattery,"%"); }
    /* signal */
    memset(b,0,sizeof b); b[3]=0x82; b[4]=60;
    if (nl_info_parse_signal(b,5,&pct)==NL_INFO_OK) { strcat(gBattery,"   Signal "); append_num(gBattery,pct); strcat(gBattery,"%"); }
    /* network */
    memset(b,0,sizeof b); b[3]=0x71; b[8]=1; b[10]=0x12; b[11]=0x34; b[12]=0xAB; b[13]=0xCD;
    b[14]=0x32; b[15]=0xF4; b[16]=0x51; b[17]=8;
    for(i=0;i<8;i++){ b[18+2*i]=0; b[19+2*i]=(nl_u8)"Itineris"[i]; }
    gNetwork[0]=0;
    if (nl_info_parse_network(b,34,&net)==NL_INFO_OK && net.registered) {
        strcpy(gNetwork, net.name); strcat(gNetwork, "  ("); strcat(gNetwork, net.code); strcat(gNetwork, ")");
    }
    /* clock */
    memset(b,0,sizeof b); b[3]=0x63; b[4]=1;
    b[8]=(2026>>8); b[9]=(2026&0xFF); b[10]=11; b[11]=17; b[12]=14; b[13]=30; b[14]=45;
    gClock[0]=0;
    if (nl_info_parse_clock(b,15,&clk)==NL_INFO_OK) {
        int da=nl_date_day(clk.date), mo=nl_date_month(clk.date), yr=nl_date_year(clk.date);
        long hh=clk.secs/3600, mi=(clk.secs/60)%60;
        append_num(gClock, da); strcat(gClock," "); strcat(gClock, mons[mo]); strcat(gClock," ");
        append_num(gClock, yr); strcat(gClock, ", ");
        if (hh<10) { strcat(gClock,"0"); }
        append_num(gClock, hh); strcat(gClock,":");
        if (mi<10) { strcat(gClock,"0"); }
        append_num(gClock, mi);
    }
}

static void DrawCStr(const char *s) { DrawText((Ptr)s, 0, (short)strlen(s)); }

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
    TextFont(3);                /* Geneva 9 throughout, so line metrics are uniform */
    TextSize(9);
    MoveTo(14, 18); DrawCStr(gModel[0] ? gModel : "No phone connected.");
    MoveTo(14, 34); DrawCStr(gFirmware);
    MoveTo(14, 50); DrawCStr(gBattery);
    MoveTo(14, 66); DrawCStr(gNetwork);
    MoveTo(14, 82); DrawCStr(gClock);
    MoveTo(14, 104); DrawCStr("(sample data \320 the serial link is not built yet)");
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

    BuildSampleStatus();
    gPhoneWindow = GetNewWindow(kPhoneWindow, nil, (WindowPtr)-1L);
}

int main(void)
{
    EventRecord event;

    Init();
#ifdef NOKIALINK_SMOKE
    Smoke_Run();        /* run the module checks, write "out", then quit */
    return 0;
#endif
    if (gPhoneWindow)
        ShowWindow(gPhoneWindow);
    while (!gQuit)
    {
        if (WaitNextEvent(everyEvent, &event, 30, nil))
            HandleEvent(&event);
    }
    return 0;
}

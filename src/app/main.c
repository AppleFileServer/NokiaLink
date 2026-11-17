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
#ifdef NOKIALINK_FAKEPHONE
#include "link.h"
#include "fakephone.h"   /* the pretend phone, standing in for the cable */
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

/* Fill the shown fields from a decoded reply body. Shared by the canned
 * path and the pretend-phone-over-the-link path. */
static void fill_identify(const nl_u8 *b, long n)
{
    nl_info_ident id;
    gModel[0] = 0; gFirmware[0] = 0;
    if (nl_info_parse_identify(b, n, &id) == NL_INFO_OK) {
        strcpy(gModel, "Nokia 7110 ("); strcat(gModel, id.model); strcat(gModel, ")");
        strcpy(gFirmware, "Firmware "); strcat(gFirmware, id.version);
        strcat(gFirmware, ", "); strcat(gFirmware, id.date);
    }
}
static void fill_battery(const nl_u8 *b, long n)
{
    int p; strcpy(gBattery, "Battery ");
    if (nl_info_parse_battery(b, n, &p) == NL_INFO_OK) { append_num(gBattery, p); strcat(gBattery, "%"); }
}
static void fill_signal(const nl_u8 *b, long n)
{
    int p;
    if (nl_info_parse_signal(b, n, &p) == NL_INFO_OK) { strcat(gBattery, "   Signal "); append_num(gBattery, p); strcat(gBattery, "%"); }
}
static void fill_network(const nl_u8 *b, long n)
{
    nl_info_network net; gNetwork[0] = 0;
    if (nl_info_parse_network(b, n, &net) == NL_INFO_OK && net.registered) {
        strcpy(gNetwork, net.name); strcat(gNetwork, "  ("); strcat(gNetwork, net.code); strcat(gNetwork, ")");
    }
}
static void fill_clock(const nl_u8 *b, long n)
{
    static const char *mons[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                                 "Jul","Aug","Sep","Oct","Nov","Dec"};
    nl_stamp clk; gClock[0] = 0;
    if (nl_info_parse_clock(b, n, &clk) == NL_INFO_OK) {
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

/* build the canned reply bodies the two paths share */
static long body_identify(nl_u8 *b)
{ const char *t="V 04.80\r23-02-00\rNSE-5\r"; long n; int i;
  memset(b,0,64); b[3]=0xD2; for(n=4,i=0;t[i];i++) b[n++]=(nl_u8)t[i]; return n; }
static void body_battery(nl_u8 *b){ memset(b,0,8); b[3]=0x03; b[5]=77; }
static void body_signal(nl_u8 *b){ memset(b,0,8); b[3]=0x82; b[4]=60; }
static void body_network(nl_u8 *b)
{ int i; memset(b,0,48); b[3]=0x71; b[8]=1; b[10]=0x12; b[11]=0x34; b[12]=0xAB; b[13]=0xCD;
  b[14]=0x32; b[15]=0xF4; b[16]=0x51; b[17]=8;
  for(i=0;i<8;i++){ b[18+2*i]=0; b[19+2*i]=(nl_u8)"Itineris"[i]; } }
static void body_clock(nl_u8 *b)
{ memset(b,0,16); b[3]=0x63; b[4]=1; b[8]=(2026>>8); b[9]=(2026&0xFF);
  b[10]=11; b[11]=17; b[12]=14; b[13]=30; b[14]=45; }

static void BuildSampleStatus(void)
{
    nl_u8 b[64]; long n;
    n = body_identify(b); fill_identify(b, n);
    body_battery(b); fill_battery(b, 6);
    body_signal(b);  fill_signal(b, 5);
    body_network(b); fill_network(b, 34);
    body_clock(b);   fill_clock(b, 15);
}

#ifdef NOKIALINK_FAKEPHONE
/* The pretend phone standing in for the cable: the window's data is
 * fetched by sending real FBUS requests through the link and decoding the
 * replies, so the whole stack runs (services + link + framing) on the 68k. */
static fakephone gFake;
static nl_link   gLink;
static nl_u8     gLinkMsg[1024];

static void add_reply(nl_u8 req, nl_u8 rep, const nl_u8 *d, long len)
{
    fp_reply r; memset(&r, 0, sizeof r);
    r.req_type = req; r.reply_type = rep; r.data = d; r.len = len; r.chunk = 200;
    fp_add_reply(&gFake, &r);
}

static void BuildStatusViaLink(void)
{
    static nl_u8 idb[64], batb[8], netb[48], clkb[16];
    nl_u8 req[16], reply[256]; long rn, len, idlen;

    idlen = body_identify(idb);
    body_battery(batb); body_network(netb); body_clock(clkb);

    fp_init(&gFake);
    add_reply(0xD1, 0xD2, idb, idlen);
    add_reply(0x17, 0x17, batb, 6);
    add_reply(0x0A, 0x0A, netb, 34);   /* network (signal shares 0x0A; left out here) */
    add_reply(0x19, 0x19, clkb, 15);
    nl_link_init(&gLink, &gFake.port, gLinkMsg, sizeof gLinkMsg, 0, 0);
    nl_link_start_dlr3(&gLink);

    gModel[0]=0; gFirmware[0]=0; strcpy(gBattery,"Battery \321"); gNetwork[0]=0; gClock[0]=0;
    rn = nl_info_req_identify(req, sizeof req);
    if (nl_link_request(&gLink, 0xD1, req, rn, 0xD2, reply, sizeof reply, &len, 3000) == NL_OK) fill_identify(reply, len);
    rn = nl_info_req_battery(req, sizeof req);
    if (nl_link_request(&gLink, 0x17, req, rn, 0x17, reply, sizeof reply, &len, 3000) == NL_OK) fill_battery(reply, len);
    rn = nl_info_req_network(req, sizeof req);
    if (nl_link_request(&gLink, 0x0A, req, rn, 0x0A, reply, sizeof reply, &len, 3000) == NL_OK) fill_network(reply, len);
    rn = nl_info_req_clock(req, sizeof req);
    if (nl_link_request(&gLink, 0x19, req, rn, 0x19, reply, sizeof reply, &len, 3000) == NL_OK) fill_clock(reply, len);

    (void)BuildSampleStatus; (void)fill_signal; (void)body_signal;
}
#endif

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
#ifdef NOKIALINK_FAKEPHONE
    MoveTo(14, 104); DrawCStr("(read through the link from the pretend phone)");
#else
    MoveTo(14, 104); DrawCStr("(sample data \320 the serial link is not built yet)");
#endif
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

#ifdef NOKIALINK_FAKEPHONE
    BuildStatusViaLink();
#else
    BuildSampleStatus();
#endif
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

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
#include "nlpbk.h"
#include "listwin.h"
#include "nlsms.h"
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


enum {
    kMenuBar = 128,
    kAppleMenu = 128, kAboutItem = 1,
    kFileMenu = 129, kQuitItem = 1,
    kEditMenu = 130,
    kViewMenu = 131, kPhoneItem = 1, kContactsItem = 2, kMessagesItem = 3,
    kCalendarItem = 4, kCallsItem = 5,
    kMainWindow = 128,
    kAboutAlert = 128
};

/* The made-up contacts the Contacts window shows. The pretend phone keeps
 * one per location, so reading them back over the link exercises the real
 * read path (request per location -> link -> decode). */
typedef struct { const char *name; nl_u8 ntype; const char *num; } sample_contact;
static const sample_contact kSample[] = {
    { "Ada Lovelace",      0x03, "07700900001" },
    { "Alan Turing",       0x02, "07700900002" },
    { "Grace Hopper",      0x00, "07700900003" },
    { "Edsger Dijkstra",   0x03, "07700900004" },
    { "Katherine Johnson", 0x03, "07700900005" }
};
#define kSampleN ((int)(sizeof kSample / sizeof kSample[0]))

/* Build the body of a phonebook-entry reply (what the phone sends back for
 * one contact): a name block (0x07) and a number block (0x0B). */
static long build_pbk_reply(nl_u8 *b, long cap, const char *name, nl_u8 ntype, const char *num)
{
    long off, bs; int i, tb;
    memset(b, 0, (size_t)cap); b[6] = 0x01; off = 18;
    tb = 2 * (int)strlen(name); bs = 6 + tb;
    b[off]=0x07; b[off+2]=(nl_u8)(bs>>8); b[off+3]=(nl_u8)bs; b[off+5]=(nl_u8)tb;
    for (i=0; name[i]; i++) { b[off+6+2*i]=0; b[off+7+2*i]=(nl_u8)name[i]; } off += bs;
    tb = 2 * (int)strlen(num); bs = 10 + tb;
    b[off]=0x0B; b[off+2]=(nl_u8)(bs>>8); b[off+3]=(nl_u8)bs; b[off+5]=ntype; b[off+9]=(nl_u8)tb;
    for (i=0; num[i]; i++) { b[off+10+2*i]=0; b[off+11+2*i]=(nl_u8)num[i]; } off += bs;
    return off;
}

/* NokiaLink runs in one window whose content switches between views. */
#define VIEW_PHONE    0
#define VIEW_CONTACTS 1
#define VIEW_MESSAGES 2
#define VIEW_CALENDAR 3
#define VIEW_CALLS    4
static nl_listwin gMain;
static short      gView;

static void add_entry_row(const nl_pbk_entry *e)
{
    char row[96];
    strcpy(row, e->name);
    if (e->nnumbers) { strcat(row, "  "); strcat(row, e->numbers[0].text); }
    listwin_add(&gMain, row);
}

/* Canned path (the normal build): decode each reply body straight away. */
static void BuildContacts(void)
{
    nl_u8 b[128]; nl_pbk_entry e; long n; int i;
    listwin_clear(&gMain);
    for (i = 0; i < kSampleN; i++) {
        n = build_pbk_reply(b, sizeof b, kSample[i].name, kSample[i].ntype, kSample[i].num);
        if (nl_pbk_parse_entry(b, n, (nl_u16)(i+1), &e) == NL_PBK_OK)
            add_entry_row(&e);
    }
}

#ifdef NOKIALINK_FAKEPHONE
/* Link path (the pretend-phone build): the pretend phone keeps one entry
 * per location, and the list is filled by asking for locations 1..N over
 * the link and decoding each reply -- the whole read path on the 68000. */
static fakephone gFakeC;
static nl_link   gLinkC;
static nl_u8     gLinkCMsg[1024];
static nl_u8     gPbkBody[8][96];   /* a held reply body per location */

static void BuildContactsViaLink(void)
{
    nl_u8 req[32], reply[256]; long rn, len, n; int i;
    nl_pbk_entry e; fp_reply r;

    listwin_clear(&gMain);
    fp_init(&gFakeC);
    for (i = 0; i < kSampleN && i < 8; i++) {
        n = build_pbk_reply(gPbkBody[i], (long)sizeof gPbkBody[i],
                            kSample[i].name, kSample[i].ntype, kSample[i].num);
        memset(&r, 0, sizeof r);
        r.req_type = 0x03; r.reply_type = 0x03; r.data = gPbkBody[i]; r.len = n; r.chunk = 200;
        r.use_body = 1; r.body_off = 10; r.body_val = (nl_u16)(i + 1);   /* match the location */
        fp_add_reply(&gFakeC, &r);
    }
    nl_link_init(&gLinkC, &gFakeC.port, gLinkCMsg, sizeof gLinkCMsg, 0, 0);
    nl_link_start_dlr3(&gLinkC);

    for (i = 0; i < kSampleN && i < 8; i++) {
        rn = nl_pbk_req_read(req, sizeof req, NL_PBK_MEM_ME, (nl_u16)(i + 1));
        if (rn > 0 &&
            nl_link_request(&gLinkC, 0x03, req, rn, 0x03, reply, sizeof reply, &len, 3000) == NL_OK &&
            nl_pbk_parse_entry(reply, len, (nl_u16)(i + 1), &e) == NL_PBK_OK)
            add_entry_row(&e);
    }
    (void)BuildContacts;
}
#endif

/* Fill the Messages window. Sample rows for now (all fictional -- this is a
 * public repo); the real list comes from the SMS read once the serial link
 * to the phone is live, decoded by nlsms as the host tool already does. */
static void BuildMessages(void)
{
    listwin_clear(&gMain);
    listwin_add(&gMain, "Ada Lovelace     See you at the lecture at 3");
    listwin_add(&gMain, "Alan Turing      Bring the papers tomorrow x");
    listwin_add(&gMain, "me > Grace       On my way, five minutes");
    listwin_add(&gMain, "Katherine J.     Numbers check out, go for launch");
    listwin_add(&gMain, "me > Edsger      Lunch at one?");
    (void)nl_sms_pdu_name;   /* nlsms is linked in for the real read later */
}

/* Fill the Calendar view. Sample rows for now (all fictional -- public
 * repo); the real list comes from the calendar read over the link, decoded
 * by nlcal as the host tool already does. */
static void BuildCalendar(void)
{
    listwin_clear(&gMain);
    listwin_add(&gMain, "Mon 23 Nov  09:00  Meeting: project review");
    listwin_add(&gMain, "Tue 24 Nov  14:30  Call: dentist");
    listwin_add(&gMain, "Wed 25 Nov         Reminder: pay rent");
    listwin_add(&gMain, "Sat 28 Nov         Birthday: Grace");
    listwin_add(&gMain, "Mon 30 Nov  18:00  Meeting: book club");
}

/* Fill the Call-log view. Sample rows for now (all fictional -- public
 * repo); the real list comes from the call-log read over the link. */
static void BuildCalls(void)
{
    listwin_clear(&gMain);
    listwin_add(&gMain, "Dialled   Ada Lovelace     17 Nov  16:42");
    listwin_add(&gMain, "Missed    07700900042      17 Nov  11:08");
    listwin_add(&gMain, "Received  Alan Turing       16 Nov  19:20");
    listwin_add(&gMain, "Dialled   Grace Hopper      16 Nov  09:15");
    listwin_add(&gMain, "Received  07700900003      15 Nov  21:03");
}

/* The stack: the Toolbox's own calls and interrupts come on top of what
   NokiaLink uses. Set before the heap is first grown. */
enum { kStackSize = 16L * 1024 };

static Boolean gQuit;

static void About(void)
{
    ParamText(kAppVersionP, "\p", "\p", "\p");
    Alert(kAboutAlert, nil);
}

/* Set the single window's title to the current view. */
static void set_title(const char *s)
{
    Str255 p; int i;
    for (i = 0; s[i] && i < 255; i++) p[i + 1] = (unsigned char)s[i];
    p[0] = (unsigned char)i;
    if (gMain.win) SetWTitle(gMain.win, p);
}

/* The Phone view is the status, shown as rows in the one window. */
static void fill_phone_view(void)
{
    listwin_clear(&gMain);
    listwin_add(&gMain, gModel[0] ? gModel : "No phone connected.");
    if (gFirmware[0]) listwin_add(&gMain, gFirmware);
    if (gBattery[0])  listwin_add(&gMain, gBattery);
    if (gNetwork[0])  listwin_add(&gMain, gNetwork);
    if (gClock[0])    listwin_add(&gMain, gClock);
#ifdef NOKIALINK_FAKEPHONE
    listwin_add(&gMain, "(read through the link from the pretend phone)");
#else
    listwin_add(&gMain, "(sample data \320 the serial link is not built yet)");
#endif
}

/* Switch the window to a view: refill the list and rename the window. */
static void SwitchView(short v)
{
    gView = v;
    switch (v) {
    case VIEW_CONTACTS:
#ifdef NOKIALINK_FAKEPHONE
        BuildContactsViaLink();
#else
        BuildContacts();
#endif
        set_title("NokiaLink \320 Contacts");
        break;
    case VIEW_MESSAGES:
        BuildMessages();
        set_title("NokiaLink \320 Messages");
        break;
    case VIEW_CALENDAR:
        BuildCalendar();
        set_title("NokiaLink \320 Calendar");
        break;
    case VIEW_CALLS:
        BuildCalls();
        set_title("NokiaLink \320 Calls");
        break;
    default:
        fill_phone_view();
        set_title("NokiaLink \320 Phone");
        break;
    }
    if (gMain.win) {
        SetPort(gMain.win);
        InvalRect(&gMain.win->portRect);
    }
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
    case kViewMenu:
        if (item == kPhoneItem) SwitchView(VIEW_PHONE);
        else if (item == kContactsItem) SwitchView(VIEW_CONTACTS);
        else if (item == kMessagesItem) SwitchView(VIEW_MESSAGES);
        else if (item == kCalendarItem) SwitchView(VIEW_CALENDAR);
        else if (item == kCallsItem) SwitchView(VIEW_CALLS);
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
        case inGoAway:
            if (TrackGoAway(window, event->where))
                HideWindow(window);
            break;
        case inContent:
            if (window != FrontWindow())
                SelectWindow(window);
            else if (listwin_owns(&gMain, window))
                listwin_click(&gMain, event);
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
        if (listwin_owns(&gMain, window))
            listwin_update(&gMain);
        break;
    case activateEvt:
        window = (WindowPtr)event->message;
        if (listwin_owns(&gMain, window))
            listwin_activate(&gMain, (event->modifiers & activeFlag) != 0);
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
    listwin_create(&gMain, kMainWindow);
    SwitchView(VIEW_PHONE);      /* the window opens on the Phone view */
    listwin_show(&gMain);
}

int main(void)
{
    EventRecord event;

    Init();
#ifdef NOKIALINK_SMOKE
    Smoke_Run();        /* run the module checks, write "out", then quit */
    return 0;
#endif
    if (gMain.win)
        ShowWindow(gMain.win);
    while (!gQuit)
    {
        if (WaitNextEvent(everyEvent, &event, 30, nil))
            HandleEvent(&event);
    }
    return 0;
}

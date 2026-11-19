/* The action dialogs (see dialogs.h). Modal dialogs over the Dialog Manager. */
#include <Dialogs.h>   /* pulls in the Control Manager too (Multiverse.h) */
#include <string.h>

#include "dialogs.h"

enum {
    kNewMsgDLOG  = 200,
    kAddApptDLOG = 201,
    kConfirmALRT = 202,
    kNoticeALRT  = 203
};

/* New Message item numbers (match DITL 200). */
enum { nmSend = 1, nmCancel = 2, nmTo = 4, nmMsg = 6 };

/* Add Appointment item numbers (match DITL 201). */
enum { apOK = 1, apCancel = 2, apMeeting = 4, apReminder = 5,
       apDate = 7, apTime = 9, apText = 11 };

/* A C string as a Pascal string. */
static void to_pstr(const char *s, Str255 p)
{
    int i;
    for (i = 0; s[i] && i < 255; i++) p[i + 1] = (unsigned char)s[i];
    p[0] = (unsigned char)i;
}

/* Read an edit-text item into a C buffer. */
static void get_text(DialogPtr d, short item, char *buf, int sz)
{
    short type; Handle h; Rect r; Str255 s; int n;
    GetDialogItem(d, item, &type, &h, &r);
    GetDialogItemText(h, s);
    n = s[0];
    if (n > sz - 1) n = sz - 1;
    if (n > 0) memcpy(buf, s + 1, (size_t)n);
    buf[n] = 0;
}

static void set_radio(DialogPtr d, short item, short on)
{
    short type; Handle h; Rect r;
    GetDialogItem(d, item, &type, &h, &r);
    if (h) SetControlValue((ControlHandle)h, on);
}

/* Return/Enter -> the default (item 1), Escape -> Cancel (item 2). */
static pascal Boolean dlg_filter(DialogPtr d, EventRecord *ev, short *item)
{
    unsigned char ch;
    (void)d;
    if (ev->what == keyDown || ev->what == autoKey) {
        ch = (unsigned char)(ev->message & charCodeMask);
        if (ch == 0x0D || ch == 0x03) { *item = 1; return true; }
        if (ch == 0x1B) { *item = 2; return true; }
    }
    return false;
}

int dlg_new_message(char *to, int tosz, char *text, int textsz)
{
    DialogPtr d;
    short item;

    d = GetNewDialog(kNewMsgDLOG, nil, (WindowPtr)-1L);
    if (!d) return 0;
    ShowWindow(d);
    do {
        ModalDialog(dlg_filter, &item);
    } while (item != nmSend && item != nmCancel);
    if (item == nmSend) {
        get_text(d, nmTo, to, tosz);
        get_text(d, nmMsg, text, textsz);
    }
    DisposeDialog(d);
    return item == nmSend;
}

int dlg_add_appt(int *type, char *date, int dsz,
                 char *tim, int tsz, char *text, int textsz)
{
    DialogPtr d;
    short item;
    int t = 0;   /* 0 = meeting, 1 = reminder */

    d = GetNewDialog(kAddApptDLOG, nil, (WindowPtr)-1L);
    if (!d) return 0;
    set_radio(d, apMeeting, 1);
    set_radio(d, apReminder, 0);
    ShowWindow(d);
    do {
        ModalDialog(dlg_filter, &item);
        if (item == apMeeting) {
            t = 0; set_radio(d, apMeeting, 1); set_radio(d, apReminder, 0);
        } else if (item == apReminder) {
            t = 1; set_radio(d, apMeeting, 0); set_radio(d, apReminder, 1);
        }
    } while (item != apOK && item != apCancel);
    if (item == apOK) {
        *type = t;
        get_text(d, apDate, date, dsz);
        get_text(d, apTime, tim, tsz);
        get_text(d, apText, text, textsz);
    }
    DisposeDialog(d);
    return item == apOK;
}

int dlg_confirm(const char *msg)
{
    Str255 p;
    to_pstr(msg, p);
    ParamText(p, "\p", "\p", "\p");
    return Alert(kConfirmALRT, dlg_filter) == 1;
}

void dlg_notice(const char *msg)
{
    Str255 p;
    to_pstr(msg, p);
    ParamText(p, "\p", "\p", "\p");
    (void)Alert(kNoticeALRT, dlg_filter);
}

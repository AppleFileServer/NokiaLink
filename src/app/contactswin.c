/* The Contacts window (see contactswin.h). List Manager, one text column. */
#include <Quickdraw.h>
#include <Windows.h>   /* the List Manager comes in via the umbrella header */
#include <string.h>

#include "contactswin.h"

#define kContactsWindow 129

static WindowPtr gWin;
static ListHandle gList;

void ContactsWin_Create(void)
{
    Rect view, dataBounds;
    Point cSize;
    GrafPtr saved;

    gWin = GetNewWindow(kContactsWindow, nil, (WindowPtr)-1L);
    if (!gWin) return;
    GetPort(&saved);
    SetPort(gWin);

    view = gWin->portRect;
    view.right -= 15;               /* leave room for the vertical scroll bar */
    SetRect(&dataBounds, 0, 0, 1, 0);   /* one column, no rows yet */
    SetPt(&cSize, 0, 0);                /* let the List Manager size the cells */
    gList = LNew(&view, &dataBounds, cSize, 0, gWin,
                 true /*drawIt*/, false /*hasGrow*/, false /*scrollH*/, true /*scrollV*/);
    SetPort(saved);
}

WindowPtr ContactsWin_Window(void) { return gWin; }

void ContactsWin_Clear(void)
{
    if (gList) LDelRow(0, 0, gList);     /* 0 count from row 0 = delete all */
}

void ContactsWin_AddRow(const char *text)
{
    short row;
    Cell c;
    if (!gList) return;
    row = (**gList).dataBounds.bottom;   /* append after the last row */
    LAddRow(1, row, gList);
    SetPt(&c, 0, row);
    LSetCell((Ptr)text, (short)strlen(text), c, gList);
}

void ContactsWin_Show(void)
{
    if (!gWin) return;
    ShowWindow(gWin);
    SelectWindow(gWin);
}

void ContactsWin_Update(void)
{
    GrafPtr saved;
    if (!gWin) return;
    GetPort(&saved);
    SetPort(gWin);
    BeginUpdate(gWin);
    EraseRect(&gWin->portRect);
    LUpdate(gWin->visRgn, gList);
    DrawGrowIcon(gWin);
    EndUpdate(gWin);
    SetPort(saved);
}

void ContactsWin_Activate(Boolean activate)
{
    if (gList) LActivate(activate, gList);
}

void ContactsWin_Click(EventRecord *ev)
{
    GrafPtr saved;
    Point p;
    if (!gWin || !gList) return;
    GetPort(&saved);
    SetPort(gWin);
    p = ev->where;
    GlobalToLocal(&p);
    LClick(p, ev->modifiers, gList);    /* selection/scroll; result unused for now */
    SetPort(saved);
}

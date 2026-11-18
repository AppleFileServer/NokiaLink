/* A scrolling list window (see listwin.h). List Manager, one text column. */
#include <Quickdraw.h>
#include <Windows.h>   /* the List Manager comes in via the umbrella header */
#include <string.h>

#include "listwin.h"

void listwin_create(nl_listwin *w, short windID)
{
    Rect view, dataBounds;
    Point cSize;
    GrafPtr saved;

    w->win = GetNewWindow(windID, nil, (WindowPtr)-1L);
    w->list = nil;
    if (!w->win) return;
    GetPort(&saved);
    SetPort(w->win);

    view = w->win->portRect;
    view.right -= 15;                   /* room for the vertical scroll bar */
    SetRect(&dataBounds, 0, 0, 1, 0);   /* one column, no rows yet */
    SetPt(&cSize, 0, 0);                /* let the List Manager size the cells */
    w->list = LNew(&view, &dataBounds, cSize, 0, w->win,
                   true /*drawIt*/, false /*hasGrow*/, false /*scrollH*/, true /*scrollV*/);
    SetPort(saved);
}

void listwin_clear(nl_listwin *w)
{
    if (w->list) LDelRow(0, 0, w->list);   /* 0 count from row 0 = delete all */
}

void listwin_add(nl_listwin *w, const char *text)
{
    short row;
    Cell c;
    if (!w->list) return;
    row = (**w->list).dataBounds.bottom;   /* append after the last row */
    LAddRow(1, row, w->list);
    SetPt(&c, 0, row);
    LSetCell((Ptr)text, (short)strlen(text), c, w->list);
}

void listwin_show(nl_listwin *w)
{
    if (!w->win) return;
    ShowWindow(w->win);
    SelectWindow(w->win);
}

int listwin_owns(const nl_listwin *w, WindowPtr win)
{
    return w->win != nil && w->win == win;
}

void listwin_update(nl_listwin *w)
{
    GrafPtr saved;
    if (!w->win) return;
    GetPort(&saved);
    SetPort(w->win);
    BeginUpdate(w->win);
    EraseRect(&w->win->portRect);
    LUpdate(w->win->visRgn, w->list);
    DrawGrowIcon(w->win);
    EndUpdate(w->win);
    SetPort(saved);
}

void listwin_activate(nl_listwin *w, Boolean activate)
{
    if (w->list) LActivate(activate, w->list);
}

void listwin_click(nl_listwin *w, EventRecord *ev)
{
    GrafPtr saved;
    Point p;
    if (!w->win || !w->list) return;
    GetPort(&saved);
    SetPort(w->win);
    p = ev->where;
    GlobalToLocal(&p);
    LClick(p, ev->modifiers, w->list);
    SetPort(saved);
}

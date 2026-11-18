/* A scrolling list window: a thin, reusable wrapper over the List Manager.
 * One text column; the caller fills it with rows. The Contacts, Messages
 * (and later Calendar, Call log) windows are all instances of this, so the
 * window behaviour lives in one place.
 */
#ifndef NL_LISTWIN_H
#define NL_LISTWIN_H

#include <Windows.h>
#include <Events.h>

typedef struct {
    WindowPtr  win;
    ListHandle list;
} nl_listwin;

/* Make the window (hidden) from WIND resource windID and its list. */
void listwin_create(nl_listwin *w, short windID);
void listwin_clear(nl_listwin *w);                 /* remove all rows */
void listwin_add(nl_listwin *w, const char *text); /* append a row */
void listwin_show(nl_listwin *w);                  /* show and bring to front */
int  listwin_owns(const nl_listwin *w, WindowPtr win);  /* is win this one? */
void listwin_update(nl_listwin *w);                /* handle an update event */
void listwin_activate(nl_listwin *w, Boolean activate);
void listwin_click(nl_listwin *w, EventRecord *ev);

#endif

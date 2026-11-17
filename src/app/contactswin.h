/* The Contacts window: a scrolling list of contacts. A thin wrapper over
 * the List Manager; the caller fills it with rows of text. */
#ifndef NL_CONTACTSWIN_H
#define NL_CONTACTSWIN_H

#include <Windows.h>
#include <Events.h>

void      ContactsWin_Create(void);        /* make the window (hidden) and its list */
WindowPtr ContactsWin_Window(void);
void      ContactsWin_Clear(void);         /* remove all rows */
void      ContactsWin_AddRow(const char *text);
void      ContactsWin_Show(void);          /* show and bring to front */
void      ContactsWin_Update(void);        /* handle an update event */
void      ContactsWin_Activate(Boolean activate);
void      ContactsWin_Click(EventRecord *ev);

#endif

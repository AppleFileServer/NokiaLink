/* The action dialogs: the forms and confirmations for the things NokiaLink
 * does to the phone (send a message, add an appointment, sync, migrate, set
 * the clock). They only gather and confirm input; the caller carries out the
 * action over the link. Kept apart from main.c so the window shell and the
 * dialogs stay separate.
 */
#ifndef NL_DIALOGS_H
#define NL_DIALOGS_H

/* New Message: fills to/text and returns 1 if Send was chosen, else 0. */
int  dlg_new_message(char *to, int tosz, char *text, int textsz);

/* Add Appointment: *type is 0 for a meeting, 1 for a reminder. Fills
 * date/time/text and returns 1 if OK was chosen, else 0. */
int  dlg_add_appt(int *type, char *date, int dsz,
                  char *tim, int tsz, char *text, int textsz);

/* A yes/no confirmation that names the write first (SPEC APP-3).
 * Returns 1 for OK, 0 for Cancel. */
int  dlg_confirm(const char *msg);

/* A one-button notice. */
void dlg_notice(const char *msg);

#endif

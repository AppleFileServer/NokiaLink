/* The rules for the 18-month check and for trusting the Plus's clock.
 *
 * Portable C, no allocation. Nothing here reads a clock or a file: the
 * caller passes in the dates, so every rule can be tested.
 * SPECIFICATION.md sections 4.4 (USE) and 4.13 (CLK).
 */
#ifndef NL_USAGE_H
#define NL_USAGE_H

#include "nldate.h"

#define NL_UNUSED_MONTHS      18        /* USE-6 */
#define NL_CLOCK_FIRST_YEAR   2026      /* CLK-1: nothing here is older */
#define NL_CLOCK_CLOSE_SECS   (5L * 60) /* CLK-3: set without asking (PROVISIONAL) */
#define NL_CLOCK_HOUR_SLACK   (5L * 60) /* how near to an hour counts as "an hour out" */

/* ---- The monthly check ------------------------------------------- */

/* USE-5: 1 if a check is due: none has ever been made, or the last one
 * was in an earlier month than today. Once a month, on or after the 1st. */
int nl_check_due(nl_date last_check, nl_date today);

/* USE-6: 1 if a Main contact should be listed: its last-used date and
 * its kept-on date are both more than 18 months before today. A date of
 * 0 (none) counts as long ago; contacts are given a date when they are
 * added (nl_last_used_when_added), so that is rare. A date exactly 18 months back is not
 * listed; the day after it is. */
int nl_contact_unused(nl_date last_used, nl_date kept_on, nl_date today);

/* USE-2: the last-used date after seeing a text or call dated `seen`:
 * the later of the two. A date in the future (after today) is ignored. */
nl_date nl_last_used_after(nl_date last_used, nl_date seen, nl_date today);

/* USE-12: the last-used date a contact starts with when it is added
 * today: the date it arrived with, if it has one, or else today. */
nl_date nl_last_used_when_added(nl_date arrived_with, nl_date today);

/* ---- The clocks -------------------------------------------------- */

/* CLK-1: the floor is the later of the last sync and the newest network
 * time stamp on an archived text (either may be all zero). */
nl_stamp nl_clock_floor(nl_stamp last_sync, nl_stamp newest_text);

/* CLK-1: 1 if the Plus's clock may be believed: not before the floor
 * and not before NL_CLOCK_FIRST_YEAR. */
int nl_clock_trusted(nl_stamp plus, nl_stamp floor);

typedef enum {
    NL_CLOCK_CONFIRM_FIRST, /* CLK-2: first meeting; the owner confirms the Plus's time */
    NL_CLOCK_SET_PHONE,     /* CLK-3: close enough; set the phone from the Plus, don't ask */
    NL_CLOCK_ASK,           /* CLK-3: they differ; show both and ask */
    NL_CLOCK_ASK_HOUR,      /* CLK-3: they differ by about an hour; ask, and say so */
    NL_CLOCK_PLUS_WRONG     /* CLK-4: the Plus isn't trusted; never set the phone from it */
} nl_clock_action;

/* What to do about the clocks at a connection. phone_set is 0 if the
 * phone reports its clock as not set (then it can't be "close"). */
nl_clock_action nl_clock_decide(nl_stamp plus, nl_stamp phone, int phone_set,
                                nl_stamp floor, int first_connection);

#endif

/* Build a stored calendar entry (a vCalendar) from a note read off the
 * phone (an nl_cal_note). This is how a 7110 calendar note becomes a file
 * in the store's Calendar folder (SPEC 6.1: vCalendar 2.1 family).
 *
 * One VEVENT per note: SUMMARY (text), DTSTART (date, with time for a
 * meeting or call), DESCRIPTION (a call note's number), RRULE (recurrence;
 * a birthday is yearly), CATEGORIES (the note kind), X-NOKIA-ALARM-BEFORE
 * and X-NOKIA-BIRTH-YEAR, and a UID. Line ends are CR, as the store uses.
 *
 * Note on the version: a vCalendar's own VERSION is 1.0 (vCard 2.1 and
 * vCalendar 1.0 are the one Versit generation the 7110 speaks); that is
 * what is written, and what a reader expects.
 *
 * Portable C, no Macintosh or POSIX calls, no allocation.
 */
#ifndef NL_CALVCS_H
#define NL_CALVCS_H

#include "nldate.h"
#include "../core/nlcal.h"

#define NL_CALVCS_OK          0
#define NL_CALVCS_ERR_SPACE (-1)

/* Build the vCalendar text for note n into out. uid must be non-empty.
 * Returns the length, or NL_CALVCS_ERR_SPACE if out is too small. */
long nl_calvcs_build(const nl_cal_note *n, const char *uid, char *out, long outsize);

#endif

/* Calendar notes over the cable (FBUS message type 0x13).
 *
 * Builds the request bodies and decodes the reply bodies for reading the
 * 7110's calendar: how many notes and where (0x3A/0x3B), the first free
 * slot (0x31/0x32), and one note (0x19/0x1A). Read-only, as Phase 1 asks
 * (docs/PLAN.md). Writing a note is a later step.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation. The
 * layout is from the gnokii project's description and the firmware map
 * (../../MacDevelopmentReferenceLibrary/Nokia/); no gnokii code is used.
 *
 * A "body" here is what the link layer carries as an FBUS type-0x13
 * message: it begins with the three-byte header {0x00,0x01,0x00}, then a
 * one-byte sub-command, as the phone expects.
 *
 * Text comes off the phone as UCS-2 and is converted to the store's Mac
 * Roman (nltext); a character Mac Roman lacks becomes '?' and is counted.
 */
#ifndef NL_CAL_H
#define NL_CAL_H

#include "../store/nltext.h"
#include "../store/nldate.h"

#define NL_CAL_MSG_TYPE   0x13

#define NL_CAL_TEXT_MAX   256   /* Mac Roman note text + zero (cable allows 255 chars) */
#define NL_CAL_NUM_MAX    48    /* call note's phone number + zero */
#define NL_CAL_MAX_LOCS   128   /* locations taken from one notes-info frame */

/* Results */
#define NL_CAL_OK          0
#define NL_CAL_ERR_SHORT  (-1)  /* the body is shorter than the fields need */
#define NL_CAL_ERR_TYPE   (-2)  /* unknown note type */
#define NL_CAL_ERR_SUB    (-3)  /* not the reply sub-command expected */
#define NL_CAL_ERR_SPACE  (-4)  /* the caller's buffer is too small */
#define NL_CAL_ERR_EMPTY  (-5)  /* the phone reports no notes */

typedef enum {
    NL_CAL_MEETING  = 0x01,
    NL_CAL_CALL     = 0x02,
    NL_CAL_BIRTHDAY = 0x04,
    NL_CAL_REMINDER = 0x08
} nl_cal_type;

typedef enum {
    NL_REC_NONE = 0, NL_REC_DAILY, NL_REC_WEEKLY,
    NL_REC_FORTNIGHTLY, NL_REC_YEARLY, NL_REC_OTHER
} nl_cal_recur;

typedef struct {
    nl_cal_type  type;
    nl_u16       location;      /* the slot this note is in */
    nl_date      date;          /* the note's date; 0 for a birthday (every year) */
    int          hour, minute;  /* meeting and call; 0 otherwise */

    int          has_alarm;
    long         alarm_before;  /* meeting/call: minutes before; birthday: seconds before */
    int          alarm_silent;  /* birthday only */

    nl_cal_recur recur;
    int          recur_hours;   /* the raw value: hours between occurrences */

    int          birth_year;    /* birthday: year of birth, 0 if none given */

    char         text[NL_CAL_TEXT_MAX];   /* Mac Roman */
    long         text_lost;               /* characters that did not fit Mac Roman */
    char         number[NL_CAL_NUM_MAX];  /* call note: the phone number (Mac Roman) */
    long         number_lost;
} nl_cal_note;

/* ---- Requests (build a body to send) --------------------------------
 * Each returns the body length, or NL_CAL_ERR_SPACE. */

/* Ask which note slots are used. `after` is 0 for the first call; on a
 * later call pass the last location already seen, to read the rest. */
long nl_cal_req_notes_info(nl_u8 *out, long outsize, nl_u16 after);

/* Ask for the note in one slot. */
long nl_cal_req_get_note(nl_u8 *out, long outsize, nl_u16 location);

/* Ask for the first free slot. */
long nl_cal_req_first_free(nl_u8 *out, long outsize);

/* ---- Replies (decode a received body) ------------------------------- */

/* Decode a notes-info reply (sub 0x3B): the total count into *total, and
 * the slot locations in this frame into locs[0..*nlocs-1]. Returns
 * NL_CAL_OK, NL_CAL_ERR_EMPTY if the phone has no notes, or an error. */
int nl_cal_parse_notes_info(const nl_u8 *body, long len,
                            nl_u16 *total, nl_u16 *locs, int maxlocs, int *nlocs);

/* Decode the first-free-slot reply (sub 0x32). */
int nl_cal_parse_first_free(const nl_u8 *body, long len, nl_u16 *location);

/* Decode a calendar-note reply (sub 0x1A) into *note. */
int nl_cal_parse_note(const nl_u8 *body, long len, nl_cal_note *note);

/* Name a note type / recurrence, for display and tests. */
const char *nl_cal_type_name(nl_cal_type t);
const char *nl_cal_recur_name(nl_cal_recur r);

#endif

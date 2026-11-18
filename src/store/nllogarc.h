/* The call log: a plain-text, tab-separated journal that is only ever added
 * to (one line per call), de-duplicated by a fingerprint over the
 * identifying fields (SPEC LOG-1/LOG-2). One record per line, so other
 * programmes can read it easily.
 *
 * Portable C, no Macintosh or POSIX calls, no allocation. The caller does
 * the file I/O; this module formats one line and computes the fingerprint.
 */
#ifndef NL_LOGARC_H
#define NL_LOGARC_H

#include "nldate.h"
#include "nlcrc.h"

#define NL_LOGARC_VERSION 1

#define NL_LOGARC_HEADER \
    "# NokiaLink call log v1\tfingerprint\tkind\tnumber\tname\tdate\n"

#define NL_LOGARC_OK          0
#define NL_LOGARC_ERR_SPACE (-1)

typedef struct {
    const char *kind;      /* "dialled", "missed" or "received" */
    const char *number;    /* the other party, Mac Roman */
    const char *name;      /* matched contact name, or "" */
    nl_date     date;      /* 0 if the phone gave none */
    long        secs;      /* time of day, used only when has_time */
    int         has_time;
} nl_log_rec;

/* The fingerprint used for de-duplication: a CRC-32 over the kind, number
 * and the time of the call (not the matched name, which can change). */
nl_u32 nl_logarc_fingerprint(const nl_log_rec *r);

/* Format one journal line (including the trailing newline) into out.
 * Returns the length, or NL_LOGARC_ERR_SPACE if out is too small. */
long nl_logarc_format(const nl_log_rec *r, char *out, long outsize);

#endif

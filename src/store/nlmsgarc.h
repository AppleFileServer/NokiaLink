/* The message archive: a plain-text, tab-separated journal that is only
 * ever added to (one line per message). It is de-duplicated by a
 * fingerprint over the identifying fields, so an interrupted or repeated
 * sync never stores a message twice (SPEC MSG-6), and it is easy for other
 * programmes to read (one record per line, tab-separated columns).
 *
 * A message's text may hold tabs and newlines, so the text column is
 * escaped: backslash, tab, newline and carriage return become \\, \t, \n
 * and \r. Nothing else is changed, so the text stays readable.
 *
 * Portable C, no Macintosh or POSIX calls, no allocation. The caller does
 * the file I/O; this module formats one line and computes the fingerprint.
 */
#ifndef NL_MSGARC_H
#define NL_MSGARC_H

#include "nldate.h"
#include "nlcrc.h"    /* nl_u32 and the fingerprint's CRC-32 */

#define NL_MSGARC_VERSION 1

/* The first line of the file: a comment naming the format and the columns. */
#define NL_MSGARC_HEADER \
    "# NokiaLink message archive v1\t" \
    "fingerprint\tfolder\tdirection\tnumber\tname\tdate\tparts\ttext\n"

#define NL_MSGARC_OK          0
#define NL_MSGARC_ERR_SPACE (-1)

typedef struct {
    const char *folder;    /* "Inbox", "Sent", ... (no tab or newline) */
    int         incoming;  /* 1 = received, 0 = sent */
    const char *number;    /* sender or recipient, Mac Roman */
    const char *name;      /* matched contact name, or "" */
    nl_date     date;      /* 0 if the phone gave none */
    long        secs;      /* time of day, used only when has_time */
    int         has_time;
    int         parts;     /* how many SMS parts were joined (>= 1) */
    const char *text;      /* Mac Roman; may contain tabs and newlines */
} nl_msg_rec;

/* The fingerprint used for de-duplication: a CRC-32 over the identifying
 * fields (folder, direction, number, date and text -- not name or parts,
 * which can change without the message being different). */
nl_u32 nl_msgarc_fingerprint(const nl_msg_rec *r);

/* Format one journal line (including the trailing newline) into out.
 * Returns the length, or NL_MSGARC_ERR_SPACE if out is too small. */
long nl_msgarc_format(const nl_msg_rec *r, char *out, long outsize);

#endif

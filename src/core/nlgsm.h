/* GSM text, numbers and timestamps (GSM 03.38 and 03.40).
 *
 * The 7110 returns a text message as a near-standard GSM frame: the
 * body text is 7-bit packed in the default alphabet, the sender and
 * service centre are semi-octet (BCD) numbers, and the time is a
 * seven-octet stamp. This module turns those into the store's forms.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation. From
 * the GSM standards in the reference library (Standards/GSM-03.38,
 * 03.40); no other project's code is used.
 */
#ifndef NL_GSM_H
#define NL_GSM_H

#include "../store/nltext.h"
#include "../store/nldate.h"

#define NL_GSM_ESC        0x1B    /* escape to the extension table */
#define NL_GSM_MAX_SEPTETS 255

#define NL_GSM_OK          0
#define NL_GSM_ERR_SPACE  (-1)
#define NL_GSM_ERR_SHORT  (-2)
#define NL_GSM_ERR_BADNUM (-3)

/* Unpack `septets` 7-bit characters from the packed bytes at `in`,
 * skipping `fill_bits` padding bits at the start (0 unless a header
 * pushed the text off a septet boundary). Writes one byte per character
 * to out. Returns the count, or NL_GSM_ERR_SPACE. */
int nl_gsm7_unpack(const nl_u8 *in, long in_len, int fill_bits,
                   int septets, nl_u8 *out, int outmax);

/* Map unpacked 7-bit characters (with the 0x1B extension) to UCS-2.
 * Returns the number of UCS-2 units, or NL_GSM_ERR_SPACE. */
long nl_gsm7_to_ucs2(const nl_u8 *chars, int n, nl_u16 *out, int outmax);

/* Packed 7-bit text straight to the store's Mac Roman: unpack, map, then
 * convert (a character Mac Roman lacks becomes '?' and is counted in
 * *lost). Returns the Mac Roman length, or an error. */
long nl_gsm7_to_macroman(const nl_u8 *in, long in_len, int fill_bits,
                         int septets, char *out, long outsize, long *lost);

/* Decode a GSM address field at `p` (within `avail` bytes) to a number
 * string (ASCII, '+' for an international number). `semioctet` is 1 for a
 * message address (length is the digit count) and 0 for the service
 * centre (length is the octet count, including the type byte). Returns
 * the string length, or an error. */
long nl_gsm_number(const nl_u8 *p, long avail, int semioctet,
                   char *out, long outsize);

/* Decode a seven-octet SMS timestamp at `p`. Fills *out (date and
 * seconds-of-day) and, if not null, *tz_quarters with the time-zone
 * offset in quarter-hours (signed). Returns NL_GSM_OK or NL_GSM_ERR_SHORT. */
int nl_gsm_timestamp(const nl_u8 *p, long avail, nl_stamp *out, int *tz_quarters);

#endif

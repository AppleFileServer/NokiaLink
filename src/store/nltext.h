/* Text between the Nokia 7110 and the Plus's store.
 *
 * The phone holds names, calendar notes and messages as UCS-2 (16-bit
 * Unicode, big-endian on the cable). The store on the Plus is Mac Roman
 * (programme P32). Every field that crosses between them passes through
 * here.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation.
 *
 * Mac Roman holds the Western European letters NokiaLink's owner uses,
 * but not Cyrillic, Greek beyond a few signs, or emoji. A character the
 * phone has and Mac Roman lacks is turned into '?' and counted, never
 * dropped in silence (SPECIFICATION.md CON-3): the caller sees the count
 * and can warn before saving or sending.
 */
#ifndef NL_TEXT_H
#define NL_TEXT_H

#ifndef NL_BASIC_TYPES
#define NL_BASIC_TYPES
typedef unsigned char  nl_u8;
typedef unsigned short nl_u16;
typedef unsigned long  nl_u32;
#endif

/* The byte put in place of a character that does not fit Mac Roman. */
#define NL_TEXT_SUBST '?'

/* UCS-2 (big-endian, `nchars` 16-bit units at `ucs2be`) to Mac Roman.
 * Writes up to outsize-1 bytes to out and a terminating zero. Returns
 * the number of Mac Roman bytes written (not counting the zero), or -1
 * if out is too small. If `lost` is not null it receives the count of
 * characters that did not fit and became '?'. A UCS-2 surrogate pair
 * (a character outside the basic plane, such as an emoji) counts as one
 * lost character. */
long nl_ucs2_to_macroman(const nl_u8 *ucs2be, long nchars,
                         char *out, long outsize, long *lost);

/* Mac Roman (`nbytes` bytes at `mac`) to UCS-2 big-endian. Writes up to
 * `outchars` 16-bit units to out (no terminator added). Returns the
 * number of units written, or -1 if out is too small. Every Mac Roman
 * byte maps to exactly one unit, so nothing is ever lost this way. */
long nl_macroman_to_ucs2(const char *mac, long nbytes,
                         nl_u16 *out, long outchars);

/* The Unicode value of one Mac Roman byte (0x00-0xFF). ASCII maps to
 * itself; 0x80-0xFF uses the Mac Roman table. */
nl_u16 nl_macroman_char(nl_u8 c);

#endif

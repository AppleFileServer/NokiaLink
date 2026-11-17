/* Text between the phone (UCS-2) and the store (Mac Roman). See nltext.h. */
#include "nltext.h"

/* Mac Roman 0x80-0xFF to Unicode. Generated from the canonical Apple
 * mapping (Python's "mac_roman" codec). ASCII 0x00-0x7F maps to itself
 * and is not in the table. */
static const nl_u16 kMacRomanHigh[128] = {
    0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1,
    0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
    0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3,
    0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
    0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
    0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
    0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211,
    0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
    0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB,
    0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
    0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1,
    0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
    0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC,
    0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7,
};

nl_u16 nl_macroman_char(nl_u8 c)
{
    return c < 0x80 ? c : kMacRomanHigh[c - 0x80];
}

/* The Mac Roman byte for a Unicode value, or 0 if there is none.
 * (0 is safe as "none": U+0000 is never a text character here.) */
static int macroman_from_unicode(nl_u16 u)
{
    int i;
    if (u >= 0x20 && u < 0x7F) return (int)u;      /* printable ASCII */
    if (u == 0x09 || u == 0x0A || u == 0x0D) return (int)u;  /* tab, LF, CR */
    for (i = 0; i < 128; i++)
        if (kMacRomanHigh[i] == u) return 0x80 + i;
    return -1;
}

long nl_ucs2_to_macroman(const nl_u8 *ucs2be, long nchars,
                         char *out, long outsize, long *lost)
{
    long i, n = 0, nlost = 0;

    if (outsize <= 0) { if (lost) *lost = 0; return -1; }
    for (i = 0; i < nchars; i++) {
        nl_u16 u = (nl_u16)((ucs2be[2 * i] << 8) | ucs2be[2 * i + 1]);
        int mac;
        /* A high surrogate (0xD800-0xDBFF) begins a character outside the
         * basic plane, such as an emoji: two units, no Mac Roman form. */
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (i + 1 < nchars) {
                nl_u16 lo = (nl_u16)((ucs2be[2 * i + 2] << 8) | ucs2be[2 * i + 3]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) i++;   /* skip the low half too */
            }
            mac = -1;
        } else {
            mac = macroman_from_unicode(u);
        }
        if (mac < 0) { mac = NL_TEXT_SUBST; nlost++; }
        if (n + 1 >= outsize) { out[0] = 0; if (lost) *lost = nlost; return -1; }
        out[n++] = (char)mac;
    }
    out[n] = 0;
    if (lost) *lost = nlost;
    return n;
}

long nl_macroman_to_ucs2(const char *mac, long nbytes,
                         nl_u16 *out, long outchars)
{
    long i;
    if (nbytes > outchars) return -1;
    for (i = 0; i < nbytes; i++)
        out[i] = nl_macroman_char((nl_u8)mac[i]);
    return nbytes;
}

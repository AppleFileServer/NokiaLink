/* GSM text, numbers and timestamps (see nlgsm.h). */
#include "nlgsm.h"

/* GSM 03.38 default alphabet: 7-bit value -> Unicode. 0x1B is the escape
 * and is handled separately. */
static const nl_u16 kDefault[128] = {
    0x0040,0x00A3,0x0024,0x00A5,0x00E8,0x00E9,0x00F9,0x00EC, /* 00-07 */
    0x00F2,0x00C7,0x000A,0x00D8,0x00F8,0x000D,0x00C5,0x00E5, /* 08-0F */
    0x0394,0x005F,0x03A6,0x0393,0x039B,0x03A9,0x03A0,0x03A8, /* 10-17 */
    0x03A3,0x0398,0x039E,0x00A0,0x00C6,0x00E6,0x00DF,0x00C9, /* 18-1F (1B=ESC) */
    0x0020,0x0021,0x0022,0x0023,0x00A4,0x0025,0x0026,0x0027, /* 20-27 */
    0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F, /* 28-2F */
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037, /* 30-37 */
    0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F, /* 38-3F */
    0x00A1,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047, /* 40-47 */
    0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F, /* 48-4F */
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057, /* 50-57 */
    0x0058,0x0059,0x005A,0x00C4,0x00D6,0x00D1,0x00DC,0x00A7, /* 58-5F */
    0x00BF,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067, /* 60-67 */
    0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F, /* 68-6F */
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077, /* 70-77 */
    0x0078,0x0079,0x007A,0x00E4,0x00F6,0x00F1,0x00FC,0x00E0  /* 78-7F */
};

/* Extension table: the 7-bit value after 0x1B -> Unicode. Anything not
 * listed shows as its default-alphabet character. */
static nl_u16 extension(nl_u8 c)
{
    switch (c) {
    case 0x0A: return 0x000C;  /* form feed */
    case 0x14: return 0x005E;  /* ^ */
    case 0x28: return 0x007B;  /* { */
    case 0x29: return 0x007D;  /* } */
    case 0x2F: return 0x005C;  /* backslash */
    case 0x3C: return 0x005B;  /* [ */
    case 0x3D: return 0x007E;  /* ~ */
    case 0x3E: return 0x005D;  /* ] */
    case 0x40: return 0x007C;  /* | */
    case 0x65: return 0x20AC;  /* euro */
    default:   return kDefault[c & 0x7F];
    }
}

int nl_gsm7_unpack(const nl_u8 *in, long in_len, int fill_bits,
                   int septets, nl_u8 *out, int outmax)
{
    int i;
    if (septets > outmax) return NL_GSM_ERR_SPACE;
    for (i = 0; i < septets; i++) {
        long bit = (long)fill_bits + (long)i * 7;
        long byte = bit >> 3;
        int shift = (int)(bit & 7);
        int v;
        if (byte >= in_len) return NL_GSM_ERR_SHORT;
        v = in[byte] >> shift;
        if (shift > 1 && byte + 1 < in_len)
            v |= in[byte + 1] << (8 - shift);
        out[i] = (nl_u8)(v & 0x7F);
    }
    return septets;
}

long nl_gsm7_to_ucs2(const nl_u8 *chars, int n, nl_u16 *out, int outmax)
{
    long m = 0;
    int i;
    for (i = 0; i < n; i++) {
        nl_u16 u;
        if (chars[i] == NL_GSM_ESC) {
            if (i + 1 < n) { u = extension(chars[++i]); }
            else u = 0x0020;                 /* a trailing escape: a space */
        } else {
            u = kDefault[chars[i] & 0x7F];
        }
        if (m >= outmax) return NL_GSM_ERR_SPACE;
        out[m++] = u;
    }
    return m;
}

long nl_gsm7_to_macroman(const nl_u8 *in, long in_len, int fill_bits,
                         int septets, char *out, long outsize, long *lost)
{
    nl_u8 sept[NL_GSM_MAX_SEPTETS];
    nl_u16 uni[NL_GSM_MAX_SEPTETS];
    nl_u8 be[2 * NL_GSM_MAX_SEPTETS];
    long nu, i;
    int ns;

    if (septets > NL_GSM_MAX_SEPTETS) return NL_GSM_ERR_SPACE;
    ns = nl_gsm7_unpack(in, in_len, fill_bits, septets, sept, NL_GSM_MAX_SEPTETS);
    if (ns < 0) return ns;
    nu = nl_gsm7_to_ucs2(sept, ns, uni, NL_GSM_MAX_SEPTETS);
    if (nu < 0) return nu;
    for (i = 0; i < nu; i++) { be[2*i] = (nl_u8)(uni[i] >> 8); be[2*i+1] = (nl_u8)uni[i]; }
    return nl_ucs2_to_macroman(be, nu, out, outsize, lost);
}

/* one BCD nibble to its character; 0xF is a filler (returns 0) */
static int bcd_digit(int nib)
{
    if (nib <= 9) return '0' + nib;
    switch (nib) {
    case 0x0A: return '*';
    case 0x0B: return '#';
    case 0x0C: return 'a';
    case 0x0D: return 'b';
    case 0x0E: return 'c';
    default:   return 0;      /* 0x0F filler */
    }
}

long nl_gsm_number(const nl_u8 *p, long avail, int semioctet,
                   char *out, long outsize)
{
    int field_len, toa, international, digits, i;
    long octets, n = 0;

    if (avail < 2) return NL_GSM_ERR_SHORT;
    field_len = p[0];
    toa = p[1];
    if ((toa & 0x80) == 0) return NL_GSM_ERR_BADNUM;   /* top bit must be set */
    international = ((toa & 0x70) == 0x10);

    if (semioctet) { digits = field_len; octets = (field_len + 1) / 2; }
    else           { octets = field_len - 1; digits = (int)(octets * 2); }
    if (octets < 0 || 2 + octets > avail) return NL_GSM_ERR_SHORT;

    if (international) { if (n + 1 >= outsize) return NL_GSM_ERR_SPACE; out[n++] = '+'; }
    for (i = 0; i < digits; i++) {
        int byte = (int)(2 + (i >> 1));
        int nib = (i & 1) ? (p[byte] >> 4) : (p[byte] & 0x0F);
        int ch = bcd_digit(nib);
        if (ch == 0) continue;                 /* skip a filler nibble */
        if (n + 1 >= outsize) return NL_GSM_ERR_SPACE;
        out[n++] = (char)ch;
    }
    out[n] = 0;
    return n;
}

/* swapped-nibble BCD octet to a value (e.g. 0x51 -> 15) */
static int swap_bcd(nl_u8 b) { return (b & 0x0F) * 10 + (b >> 4); }

int nl_gsm_timestamp(const nl_u8 *p, long avail, nl_stamp *out, int *tz_quarters)
{
    int yy, mo, da, hh, mi, se, tzb, tz;
    if (avail < 7) return NL_GSM_ERR_SHORT;
    yy = swap_bcd(p[0]); mo = swap_bcd(p[1]); da = swap_bcd(p[2]);
    hh = swap_bcd(p[3]); mi = swap_bcd(p[4]); se = swap_bcd(p[5]);
    /* time zone: the sign is bit 3 of the first (low) nibble */
    tzb = p[6];
    tz = ((tzb & 0x08) ? -1 : 1) * (((tzb & 0x07) * 10) + (tzb >> 4));
    if (tz_quarters) *tz_quarters = tz;
    out->date = nl_date_make(2000 + yy, mo, da);
    out->secs = (long)hh * 3600 + (long)mi * 60 + se;
    if (out->date == 0) return NL_GSM_ERR_SHORT;   /* not a real date */
    return NL_GSM_OK;
}

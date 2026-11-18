/* The call-log journal (see nllogarc.h). */
#include <string.h>

#include "nllogarc.h"

static int put(char *out, long cap, long *n, const char *s)
{
    while (*s) { if (*n + 1 >= cap) return 0; out[(*n)++] = *s++; }
    return 1;
}

static int put1(char *out, long cap, long *n, char c)
{
    if (*n + 1 >= cap) return 0;
    out[(*n)++] = c;
    return 1;
}

static int put2(char *out, long cap, long *n, int v)
{
    if (!put1(out, cap, n, (char)('0' + (v / 10) % 10))) return 0;
    return put1(out, cap, n, (char)('0' + v % 10));
}

static int put4(char *out, long cap, long *n, int v)
{
    if (!put2(out, cap, n, (v / 100) % 100)) return 0;
    return put2(out, cap, n, v % 100);
}

static int put_hex(char *out, long cap, long *n, nl_u32 v)
{
    static const char h[] = "0123456789abcdef";
    int i;
    for (i = 28; i >= 0; i -= 4)
        if (!put1(out, cap, n, h[(v >> i) & 0xF])) return 0;
    return 1;
}

/* "YYYY-MM-DD HH:MM:SS", or "YYYY-MM-DD", or "" */
static int put_date(char *out, long cap, long *n, const nl_log_rec *r)
{
    if (!r->date) return 1;
    if (!put4(out, cap, n, nl_date_year(r->date))) return 0;
    if (!put1(out, cap, n, '-')) return 0;
    if (!put2(out, cap, n, nl_date_month(r->date))) return 0;
    if (!put1(out, cap, n, '-')) return 0;
    if (!put2(out, cap, n, nl_date_day(r->date))) return 0;
    if (r->has_time) {
        if (!put1(out, cap, n, ' ')) return 0;
        if (!put2(out, cap, n, (int)(r->secs / 3600))) return 0;
        if (!put1(out, cap, n, ':')) return 0;
        if (!put2(out, cap, n, (int)((r->secs / 60) % 60))) return 0;
        if (!put1(out, cap, n, ':')) return 0;
        if (!put2(out, cap, n, (int)(r->secs % 60))) return 0;
    }
    return 1;
}

nl_u32 nl_logarc_fingerprint(const nl_log_rec *r)
{
    nl_u32 c = nl_crc32_begin();
    unsigned char db[8];
    nl_u32 date = (nl_u32)r->date, secs = (nl_u32)r->secs;

    c = nl_crc32_add(c, r->kind, (long)strlen(r->kind));
    c = nl_crc32_add(c, r->number, (long)strlen(r->number));
    db[0] = (unsigned char)(date >> 24); db[1] = (unsigned char)(date >> 16);
    db[2] = (unsigned char)(date >> 8);  db[3] = (unsigned char)date;
    db[4] = (unsigned char)(secs >> 24); db[5] = (unsigned char)(secs >> 16);
    db[6] = (unsigned char)(secs >> 8);  db[7] = (unsigned char)secs;
    c = nl_crc32_add(c, db, 8);
    return nl_crc32_end(c);
}

long nl_logarc_format(const nl_log_rec *r, char *out, long outsize)
{
    long n = 0;
    if (!put_hex(out, outsize, &n, nl_logarc_fingerprint(r))) return NL_LOGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_LOGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->kind)) return NL_LOGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_LOGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->number)) return NL_LOGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_LOGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->name ? r->name : "")) return NL_LOGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_LOGARC_ERR_SPACE;
    if (!put_date(out, outsize, &n, r)) return NL_LOGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\n')) return NL_LOGARC_ERR_SPACE;
    out[n] = 0;
    return n;
}

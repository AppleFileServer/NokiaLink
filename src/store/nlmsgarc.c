/* The message archive journal (see nlmsgarc.h). */
#include <string.h>

#include "nlmsgarc.h"
#include "nlcrc.h"

/* ---- bounded output helpers ---------------------------------------- */

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

/* text column: escape backslash, tab, newline and carriage return. */
static int put_escaped(char *out, long cap, long *n, const char *s)
{
    for (; *s; s++) {
        const char *rep = 0;
        switch (*s) {
        case '\\': rep = "\\\\"; break;
        case '\t': rep = "\\t";  break;
        case '\n': rep = "\\n";  break;
        case '\r': rep = "\\r";  break;
        default: break;
        }
        if (rep) { if (!put(out, cap, n, rep)) return 0; }
        else     { if (!put1(out, cap, n, *s)) return 0; }
    }
    return 1;
}

static int put_u32hex(char *out, long cap, long *n, nl_u32 v)
{
    static const char h[] = "0123456789abcdef";
    int i;
    for (i = 28; i >= 0; i -= 4)
        if (!put1(out, cap, n, h[(v >> i) & 0xF])) return 0;
    return 1;
}

/* a small non-negative integer */
static int put_int(char *out, long cap, long *n, long v)
{
    char buf[16]; int i = 0;
    if (v < 0) v = 0;
    do { buf[i++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v && i < 15);
    while (i > 0) if (!put1(out, cap, n, buf[--i])) return 0;
    return 1;
}

static int put_two(char *out, long cap, long *n, int v)
{
    if (!put1(out, cap, n, (char)('0' + (v / 10) % 10))) return 0;
    if (!put1(out, cap, n, (char)('0' + v % 10))) return 0;
    return 1;
}

/* "YYYY-MM-DD" or "YYYY-MM-DD HH:MM" or "" */
static int put_date(char *out, long cap, long *n, const nl_msg_rec *r)
{
    int y, mo, d;
    if (!r->date) return 1;    /* leave the column empty */
    y = nl_date_year(r->date); mo = nl_date_month(r->date); d = nl_date_day(r->date);
    if (!put_int(out, cap, n, (long)y)) return 0;
    if (!put1(out, cap, n, '-')) return 0;
    if (!put_two(out, cap, n, mo)) return 0;
    if (!put1(out, cap, n, '-')) return 0;
    if (!put_two(out, cap, n, d)) return 0;
    if (r->has_time) {
        long hh = r->secs / 3600, mi = (r->secs / 60) % 60;
        if (!put1(out, cap, n, ' ')) return 0;
        if (!put_two(out, cap, n, (int)hh)) return 0;
        if (!put1(out, cap, n, ':')) return 0;
        if (!put_two(out, cap, n, (int)mi)) return 0;
    }
    return 1;
}

/* ---- fingerprint --------------------------------------------------- */

nl_u32 nl_msgarc_fingerprint(const nl_msg_rec *r)
{
    nl_u32 c = nl_crc32_begin();
    unsigned char d = (unsigned char)(r->incoming ? 'R' : 'S');
    unsigned char db[8];
    nl_u32 date = (nl_u32)r->date;
    nl_u32 secs = (nl_u32)r->secs;

    c = nl_crc32_add(c, r->folder, (long)strlen(r->folder));
    c = nl_crc32_add(c, &d, 1);
    c = nl_crc32_add(c, r->number, (long)strlen(r->number));
    db[0] = (unsigned char)(date >> 24); db[1] = (unsigned char)(date >> 16);
    db[2] = (unsigned char)(date >> 8);  db[3] = (unsigned char)date;
    db[4] = (unsigned char)(secs >> 24); db[5] = (unsigned char)(secs >> 16);
    db[6] = (unsigned char)(secs >> 8);  db[7] = (unsigned char)secs;
    c = nl_crc32_add(c, db, 8);
    c = nl_crc32_add(c, r->text, (long)strlen(r->text));
    return nl_crc32_end(c);
}

/* ---- one journal line ---------------------------------------------- */

long nl_msgarc_format(const nl_msg_rec *r, char *out, long outsize)
{
    long n = 0;
    if (!put_u32hex(out, outsize, &n, nl_msgarc_fingerprint(r))) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->folder)) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->incoming ? "received" : "sent")) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->number)) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put(out, outsize, &n, r->name ? r->name : "")) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put_date(out, outsize, &n, r)) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put_int(out, outsize, &n, (long)(r->parts > 0 ? r->parts : 1))) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\t')) return NL_MSGARC_ERR_SPACE;
    if (!put_escaped(out, outsize, &n, r->text)) return NL_MSGARC_ERR_SPACE;
    if (!put1(out, outsize, &n, '\n')) return NL_MSGARC_ERR_SPACE;
    out[n] = 0;
    return n;
}

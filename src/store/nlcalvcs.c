/* Build a stored vCalendar from a phone calendar note (see nlcalvcs.h). */
#include <string.h>

#include "nlcalvcs.h"

static int sput(char *o, long cap, long *n, const char *s)
{
    while (*s) { if (*n + 1 >= cap) return 0; o[(*n)++] = *s++; }
    return 1;
}

static int sc(char *o, long cap, long *n, char c)
{
    if (*n + 1 >= cap) return 0;
    o[(*n)++] = c;
    return 1;
}

/* A "NAME:value" line ending in CR; CR/LF inside the value become spaces. */
static int line(char *o, long cap, long *n, const char *name, const char *val)
{
    if (!sput(o, cap, n, name)) return 0;
    if (!sc(o, cap, n, ':')) return 0;
    for (; *val; val++) {
        char c = (*val == '\r' || *val == '\n') ? ' ' : *val;
        if (!sc(o, cap, n, c)) return 0;
    }
    return sc(o, cap, n, '\r');
}

static void fmt2(char *o, int v) { o[0] = (char)('0' + (v / 10) % 10); o[1] = (char)('0' + v % 10); }
static void fmt4(char *o, int v) { fmt2(o, (v / 100) % 100); fmt2(o + 2, v % 100); }

static void utostr(long v, char *out)
{
    char tmp[16]; int i = 0, j = 0;
    if (v < 0) v = 0;
    do { tmp[i++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v && i < 15);
    while (i > 0) out[j++] = tmp[--i];
    out[j] = 0;
}

static const char *categoryof(nl_cal_type t)
{
    switch (t) {
    case NL_CAL_MEETING:  return "MEETING";
    case NL_CAL_CALL:     return "CALL";
    case NL_CAL_BIRTHDAY: return "BIRTHDAY";
    case NL_CAL_REMINDER: return "REMINDER";
    default:              return "OTHER";
    }
}

static const char *ruleof(nl_cal_recur r)
{
    switch (r) {
    case NL_REC_DAILY:       return "D1";
    case NL_REC_WEEKLY:      return "W1";
    case NL_REC_FORTNIGHTLY: return "W2";
    case NL_REC_YEARLY:      return "YM1";
    default:                 return 0;
    }
}

long nl_calvcs_build(const nl_cal_note *nn, const char *uid, char *out, long outsize)
{
    long n = 0;
    char dt[20];
    const char *rr;

    if (!sput(out, outsize, &n, "BEGIN:VCALENDAR\rVERSION:1.0\rBEGIN:VEVENT\r"))
        return NL_CALVCS_ERR_SPACE;
    if (!line(out, outsize, &n, "UID", uid)) return NL_CALVCS_ERR_SPACE;
    if (nn->text[0] && !line(out, outsize, &n, "SUMMARY", nn->text))
        return NL_CALVCS_ERR_SPACE;

    if (nn->date) {
        int y = nl_date_year(nn->date);
        if (nn->type == NL_CAL_BIRTHDAY && nn->birth_year) y = nn->birth_year;
        fmt4(dt, y); fmt2(dt + 4, nl_date_month(nn->date)); fmt2(dt + 6, nl_date_day(nn->date));
        dt[8] = 0;
        if (nn->type == NL_CAL_MEETING || nn->type == NL_CAL_CALL) {
            dt[8] = 'T'; fmt2(dt + 9, nn->hour); fmt2(dt + 11, nn->minute);
            dt[13] = '0'; dt[14] = '0'; dt[15] = 0;
        }
        if (!line(out, outsize, &n, "DTSTART", dt)) return NL_CALVCS_ERR_SPACE;
    }

    if (nn->type == NL_CAL_CALL && nn->number[0] &&
        !line(out, outsize, &n, "DESCRIPTION", nn->number))
        return NL_CALVCS_ERR_SPACE;

    rr = (nn->type == NL_CAL_BIRTHDAY) ? "YM1" : ruleof(nn->recur);
    if (rr && !line(out, outsize, &n, "RRULE", rr)) return NL_CALVCS_ERR_SPACE;

    if (nn->has_alarm) {
        char num[16];
        utostr(nn->alarm_before, num);   /* minutes (meeting/call) or seconds (birthday) */
        if (!line(out, outsize, &n, "X-NOKIA-ALARM-BEFORE", num)) return NL_CALVCS_ERR_SPACE;
    }
    if (nn->type == NL_CAL_BIRTHDAY && nn->birth_year) {
        char yb[8]; fmt4(yb, nn->birth_year); yb[4] = 0;
        if (!line(out, outsize, &n, "X-NOKIA-BIRTH-YEAR", yb)) return NL_CALVCS_ERR_SPACE;
    }

    if (!line(out, outsize, &n, "CATEGORIES", categoryof(nn->type))) return NL_CALVCS_ERR_SPACE;
    if (!sput(out, outsize, &n, "END:VEVENT\rEND:VCALENDAR\r")) return NL_CALVCS_ERR_SPACE;
    out[n] = 0;
    return n;
}

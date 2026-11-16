/* Dates and times (nldate.h). */
#include "nldate.h"

int nl_date_year(nl_date d)  { return (int)(d / 10000); }
int nl_date_month(nl_date d) { return (int)(d / 100 % 100); }
int nl_date_day(nl_date d)   { return (int)(d % 100); }

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int nl_days_in_month(int year, int month)
{
    static const signed char days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    return month == 2 && leap(year) ? 29 : days[month - 1];
}

nl_date nl_date_make(int year, int month, int day)
{
    if (year < 1904 || year > 9999 || month < 1 || month > 12) return 0;
    if (day < 1 || day > nl_days_in_month(year, month)) return 0;
    return (nl_date)year * 10000 + month * 100 + day;
}

int nl_date_valid(nl_date d)
{
    return d > 0 && nl_date_make(nl_date_year(d), nl_date_month(d), nl_date_day(d)) == d;
}

/* Days from 1 March of year 0 to the given date, by the usual rule
 * (years counted from March, so that a leap day ends its year). */
static long civil_days(int y, int m, int d)
{
    long yy = y - (m <= 2);
    long era = yy / 400;
    long yoe = yy - era * 400;
    long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + doe;
}

#define DAYS_1904 695361L       /* civil_days(1904, 1, 1) */

long nl_date_to_days(nl_date d)
{
    if (!nl_date_valid(d)) return 0;
    return civil_days(nl_date_year(d), nl_date_month(d), nl_date_day(d)) - DAYS_1904;
}

nl_date nl_date_from_days(long days)
{
    long z = days + DAYS_1904;
    long era = z / 146097L;
    long doe = z - era * 146097L;
    long yoe = (doe - doe / 1460 + doe / 36524L - doe / 146096L) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    int d = (int)(doy - (153 * mp + 2) / 5 + 1);
    int m = (int)(mp < 10 ? mp + 3 : mp - 9);
    int y = (int)(yoe + era * 400 + (m <= 2));
    if (days < 0) return 0;
    return nl_date_make(y, m, d);
}

nl_date nl_date_add_months(nl_date d, int n)
{
    long months;
    int y, m, day, last;
    if (!nl_date_valid(d)) return 0;
    months = (long)nl_date_year(d) * 12 + (nl_date_month(d) - 1) + n;
    if (months < 0) return 0;
    y = (int)(months / 12);
    m = (int)(months % 12) + 1;
    day = nl_date_day(d);
    last = nl_days_in_month(y, m);
    if (day > last) day = last;
    return nl_date_make(y, m, day);
}

nl_date nl_date_parse(const char *text, long len)
{
    long v = 0;
    int i;
    if (!text) return 0;
    if (len < 0) for (len = 0; text[len]; len++) { if (len > 8) return 0; }
    if (len != 8) return 0;
    for (i = 0; i < 8; i++) {
        if (text[i] < '0' || text[i] > '9') return 0;
        v = v * 10 + (text[i] - '0');
    }
    return nl_date_valid(v) ? v : 0;
}

void nl_date_format(nl_date d, char *out)
{
    int i;
    if (!nl_date_valid(d)) { out[0] = 0; return; }
    for (i = 7; i >= 0; i--) { out[i] = (char)('0' + d % 10); d /= 10; }
    out[8] = 0;
}

int nl_stamp_cmp(nl_stamp a, nl_stamp b)
{
    if (a.date != b.date) return a.date < b.date ? -1 : 1;
    if (a.secs != b.secs) return a.secs < b.secs ? -1 : 1;
    return 0;
}

long nl_stamp_diff(nl_stamp a, nl_stamp b)
{
    long days = nl_date_to_days(a.date) - nl_date_to_days(b.date);
    if (days > 22000L) days = 22000L;
    if (days < -22000L) days = -22000L;
    return days * 86400L + (a.secs - b.secs);
}

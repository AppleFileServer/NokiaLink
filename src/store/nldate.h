/* Dates and times for the store and the rules that depend on them.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation.
 *
 * A date is a whole number written as it reads: 20261116 is 16 November
 * 2026, the form the store uses (SPECIFICATION.md section 6.4). 0 means
 * "no date". A stamp is a date with the seconds since midnight.
 */
#ifndef NL_DATE_H
#define NL_DATE_H

typedef long nl_date;                 /* YYYYMMDD, or 0 for none */

typedef struct {
    nl_date date;
    long    secs;                     /* 0 .. 86399 */
} nl_stamp;

nl_date nl_date_make(int year, int month, int day);   /* 0 if not a real date */
int     nl_date_year(nl_date d);
int     nl_date_month(nl_date d);
int     nl_date_day(nl_date d);
int     nl_date_valid(nl_date d);                      /* a real calendar date, 1904 or later */
int     nl_days_in_month(int year, int month);

/* Days since 1 January 1904 (the Macintosh's own starting point). */
long    nl_date_to_days(nl_date d);
nl_date nl_date_from_days(long days);

/* The date n months later (n may be negative). A day past the end of
 * the new month becomes its last day: 31 August less 18 months is
 * 28 or 29 February. */
nl_date nl_date_add_months(nl_date d, int n);

/* "YYYYMMDD" to a date; 0 unless it is exactly eight digits naming a
 * real date. len < 0 means the text ends with a zero byte. */
nl_date nl_date_parse(const char *text, long len);
/* Writes eight digits and a zero byte (nothing but the zero for 0).
 * out must hold nine bytes. */
void    nl_date_format(nl_date d, char *out);

/* < 0, 0 or > 0 as a is before, the same as or after b. */
int     nl_stamp_cmp(nl_stamp a, nl_stamp b);
/* a minus b in seconds, held to about +/- 60 years so it fits 32 bits. */
long    nl_stamp_diff(nl_stamp a, nl_stamp b);

#endif

/* Host tests for the store modules (src/store): dates, telephone number
 * matching, the 18-month and clock rules, and contact cards. Run by
 * tests/run-host.sh. All names and numbers here are made up. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nldate.h"
#include "../src/store/nlnumber.h"
#include "../src/store/nlusage.h"
#include "../src/store/nlvcard.h"

static int checks, failures;
static const char *current;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #cond); } } while (0)

static nl_stamp at(nl_date d, int h, int m)
{
    nl_stamp s;
    s.date = d;
    s.secs = h * 3600L + m * 60L;
    return s;
}

/* ---- Dates ------------------------------------------------------- */

static void test_dates(void)
{
    char buf[9];
    long d;

    current = "dates";
    CHECK(nl_date_make(2026, 11, 16) == 20261116L);
    CHECK(nl_date_make(2026, 2, 29) == 0);
    CHECK(nl_date_make(2028, 2, 29) == 20280229L);
    CHECK(nl_date_make(2100, 2, 29) == 0);          /* not a leap year */
    CHECK(nl_date_make(2000, 2, 29) == 20000229L);
    CHECK(nl_date_make(1903, 12, 31) == 0);
    CHECK(nl_date_make(2026, 13, 1) == 0);
    CHECK(nl_date_make(2026, 4, 31) == 0);
    CHECK(nl_date_valid(20261116L) && !nl_date_valid(20261131L) && !nl_date_valid(0));
    CHECK(nl_date_year(20261116L) == 2026 && nl_date_month(20261116L) == 11
          && nl_date_day(20261116L) == 16);

    CHECK(nl_date_to_days(19040101L) == 0);
    CHECK(nl_date_to_days(19040102L) == 1);
    CHECK(nl_date_to_days(19050101L) == 366);       /* 1904 was a leap year */
    CHECK(nl_date_to_days(20261116L) - nl_date_to_days(20261016L) == 31);
    CHECK(nl_date_to_days(20280301L) - nl_date_to_days(20280228L) == 2);
    for (d = 0; d < 60000L; d += 97)                /* there and back, 1904 to 2068 */
        if (nl_date_to_days(nl_date_from_days(d)) != d) break;
    CHECK(d >= 60000L);
    CHECK(nl_date_from_days(0) == 19040101L);
    CHECK(nl_date_from_days(-1) == 0);

    CHECK(nl_date_add_months(20261116L, -18) == 20250516L);
    CHECK(nl_date_add_months(20260831L, -18) == 20250228L);   /* clamped */
    CHECK(nl_date_add_months(20290831L, -18) == 20280229L);   /* to a leap day */
    CHECK(nl_date_add_months(20260131L, 1) == 20260228L);
    CHECK(nl_date_add_months(20261231L, 1) == 20270131L);
    CHECK(nl_date_add_months(20260115L, -1) == 20251215L);
    CHECK(nl_date_add_months(20260115L, 0) == 20260115L);
    CHECK(nl_date_add_months(0, 3) == 0);

    CHECK(nl_date_parse("20261116", -1) == 20261116L);
    CHECK(nl_date_parse("20261116xx", 8) == 20261116L);
    CHECK(nl_date_parse("2026111", -1) == 0);
    CHECK(nl_date_parse("202611160", -1) == 0);
    CHECK(nl_date_parse("2026-11-", -1) == 0);
    CHECK(nl_date_parse("20261131", -1) == 0);
    CHECK(nl_date_parse("", -1) == 0);
    nl_date_format(20260105L, buf);
    CHECK(strcmp(buf, "20260105") == 0);
    nl_date_format(0, buf);
    CHECK(buf[0] == 0);

    CHECK(nl_stamp_cmp(at(20261116L, 9, 0), at(20261116L, 9, 1)) < 0);
    CHECK(nl_stamp_cmp(at(20261117L, 0, 0), at(20261116L, 23, 59)) > 0);
    CHECK(nl_stamp_cmp(at(20261116L, 9, 0), at(20261116L, 9, 0)) == 0);
    CHECK(nl_stamp_diff(at(20261117L, 0, 1), at(20261116L, 23, 59)) == 120);
    CHECK(nl_stamp_diff(at(20261116L, 9, 0), at(20261116L, 10, 0)) == -3600);
    CHECK(nl_stamp_diff(at(20261116L, 0, 0), at(19040101L, 0, 0)) == 22000L * 86400L);
}

/* ---- Numbers ----------------------------------------------------- */

static void test_numbers(void)
{
    char digits[NL_NUMBER_MAX];

    current = "numbers";
    CHECK(nl_number_digits("+44 (0)7700 900-123", digits) == 13);
    CHECK(strcmp(digits, "4407700900123") == 0);
    CHECK(nl_number_digits("", digits) == 0 && digits[0] == 0);
    CHECK(nl_number_digits("123456789012345678901234567890", digits) == NL_NUMBER_MAX - 1);

    CHECK(nl_number_same("07700 900123", "+447700900123"));
    CHECK(nl_number_same("07700900123", "00447700900123"));
    CHECK(nl_number_same("+44 7700 900123", "+447700900123"));
    CHECK(!nl_number_same("07700 900123", "07700 900124"));
    CHECK(!nl_number_same("07700 900123", "07701 900123"));
    CHECK(nl_number_same("01632 960123", "+441632960123"));
    CHECK(nl_number_same("960 1234", "9601234"));            /* seven digits, no code */
    CHECK(nl_number_same("38885", "38885"));                 /* a short code */
    CHECK(!nl_number_same("38885", "138885"));
    CHECK(!nl_number_same("38885", "07700 938885"));         /* never by its tail */
    CHECK(!nl_number_same("", "07700 900123"));
    CHECK(!nl_number_same("", ""));
    CHECK(!nl_number_same("Withheld", "07700 900123"));
}

/* ---- The 18-month check ------------------------------------------ */

static void test_usage(void)
{
    current = "check due";
    CHECK(nl_check_due(0, 20261116L));                       /* never checked */
    CHECK(!nl_check_due(20261101L, 20261116L));              /* done this month */
    CHECK(!nl_check_due(20261116L, 20261130L));
    CHECK(nl_check_due(20261116L, 20261201L));               /* the 1st of the next */
    CHECK(nl_check_due(20261116L, 20270105L));               /* first start after it */
    CHECK(nl_check_due(20261231L, 20270101L));
    CHECK(!nl_check_due(20261116L, 20261016L));              /* clock gone back: no */
    CHECK(!nl_check_due(20261116L, 0));

    current = "unused";
    /* Today 16 November 2026: the line is 16 May 2025. */
    CHECK(!nl_contact_unused(20250516L, 0, 20261116L));      /* exactly 18 months: not yet */
    CHECK(nl_contact_unused(20250515L, 0, 20261116L));       /* the day before: listed */
    CHECK(!nl_contact_unused(20250517L, 0, 20261116L));
    CHECK(!nl_contact_unused(20200101L, 20260101L, 20261116L)); /* kept on, recently */
    CHECK(nl_contact_unused(20200101L, 20250101L, 20261116L));  /* both long ago */
    CHECK(!nl_contact_unused(20261001L, 20200101L, 20261116L));
    CHECK(nl_contact_unused(0, 0, 20261116L));               /* no date at all */
    CHECK(!nl_contact_unused(0, 20260601L, 20261116L));
    CHECK(!nl_contact_unused(20200101L, 0, 0));              /* no usable today */
    /* Month ends: today 31 August 2026, the line is 28 February 2025. */
    CHECK(!nl_contact_unused(20250228L, 0, 20260831L));
    CHECK(nl_contact_unused(20250227L, 0, 20260831L));
    /* A leap day: today 31 August 2029, the line is 29 February 2028. */
    CHECK(!nl_contact_unused(20280229L, 0, 20290831L));
    CHECK(nl_contact_unused(20280228L, 0, 20290831L));

    current = "last used";
    CHECK(nl_last_used_after(20260101L, 20260301L, 20261116L) == 20260301L);
    CHECK(nl_last_used_after(20260301L, 20260101L, 20261116L) == 20260301L);
    CHECK(nl_last_used_after(0, 20260101L, 20261116L) == 20260101L);
    CHECK(nl_last_used_after(20260101L, 20270101L, 20261116L) == 20260101L);  /* future: ignored */
    CHECK(nl_last_used_after(20260101L, 0, 20261116L) == 20260101L);
    CHECK(nl_last_used_after(20260101L, 20261116L, 20261116L) == 20261116L);  /* today counts */

    current = "added";
    CHECK(nl_last_used_when_added(0, 20261116L) == 20261116L);
    CHECK(nl_last_used_when_added(20260301L, 20261116L) == 20260301L);  /* keeps its own */
    CHECK(nl_last_used_when_added(20270101L, 20261116L) == 20261116L);  /* a future date: no */
    CHECK(nl_last_used_when_added(0, 0) == 0);
    CHECK(!nl_contact_unused(nl_last_used_when_added(0, 20261116L), 0, 20261201L));
}

/* ---- The clocks -------------------------------------------------- */

static void test_clocks(void)
{
    nl_stamp none = at(0, 0, 0);
    nl_stamp sync = at(20261110L, 18, 0), text = at(20261114L, 22, 33);
    nl_stamp floor = nl_clock_floor(sync, text);
    nl_stamp now = at(20261116L, 9, 0);

    current = "clocks";
    CHECK(nl_stamp_cmp(floor, text) == 0);
    CHECK(nl_stamp_cmp(nl_clock_floor(text, sync), text) == 0);
    CHECK(nl_stamp_cmp(nl_clock_floor(sync, none), sync) == 0);

    CHECK(nl_clock_trusted(now, floor));
    CHECK(nl_clock_trusted(text, floor));                    /* exactly at the floor */
    CHECK(!nl_clock_trusted(at(20261114L, 22, 32), floor));  /* a minute before it */
    CHECK(!nl_clock_trusted(at(19040101L, 0, 5), floor));    /* a flat battery */
    CHECK(!nl_clock_trusted(at(19560828L, 3, 0), none));     /* old, even with no floor */
    CHECK(!nl_clock_trusted(at(20251231L, 23, 59), none));
    CHECK(nl_clock_trusted(at(20260101L, 0, 0), none));
    CHECK(!nl_clock_trusted(none, none));

    CHECK(nl_clock_decide(now, now, 1, floor, 1) == NL_CLOCK_CONFIRM_FIRST);
    CHECK(nl_clock_decide(at(19040101L, 0, 5), now, 1, floor, 1) == NL_CLOCK_CONFIRM_FIRST);
    CHECK(nl_clock_decide(now, at(20261116L, 9, 3), 1, floor, 0) == NL_CLOCK_SET_PHONE);
    CHECK(nl_clock_decide(now, at(20261116L, 8, 55), 1, floor, 0) == NL_CLOCK_SET_PHONE);
    CHECK(nl_clock_decide(now, at(20261116L, 9, 5), 1, floor, 0) == NL_CLOCK_SET_PHONE);
    CHECK(nl_clock_decide(now, at(20261116L, 9, 6), 1, floor, 0) == NL_CLOCK_ASK);
    CHECK(nl_clock_decide(now, at(20261116L, 10, 0), 1, floor, 0) == NL_CLOCK_ASK_HOUR);
    CHECK(nl_clock_decide(now, at(20261116L, 8, 2), 1, floor, 0) == NL_CLOCK_ASK_HOUR);
    CHECK(nl_clock_decide(now, at(20261116L, 11, 0), 1, floor, 0) == NL_CLOCK_ASK);
    CHECK(nl_clock_decide(now, at(20261115L, 9, 0), 1, floor, 0) == NL_CLOCK_ASK);
    CHECK(nl_clock_decide(now, now, 0, floor, 0) == NL_CLOCK_ASK);       /* phone's clock unset */
    CHECK(nl_clock_decide(now, none, 1, floor, 0) == NL_CLOCK_ASK);
    CHECK(nl_clock_decide(at(19040101L, 0, 5), now, 1, floor, 0) == NL_CLOCK_PLUS_WRONG);
    CHECK(nl_clock_decide(at(20261113L, 9, 0), at(20261113L, 9, 0), 1, floor, 0)
          == NL_CLOCK_PLUS_WRONG);                           /* before the floor, though they agree */
    /* Midnight between them. */
    CHECK(nl_clock_decide(at(20261117L, 0, 1), at(20261116L, 23, 58), 1, floor, 0)
          == NL_CLOCK_SET_PHONE);
}

/* ---- Cards ------------------------------------------------------- */

static const char CARD_CRLF[] =
    "BEGIN:VCARD\r\n"
    "VERSION:2.1\r\n"
    "N:Example;Alex;;;\r\n"
    "FN:Alex Example\r\n"
    "TEL;CELL;VOICE:+447700900123\r\n"
    "TEL;HOME:01632 960123\r\n"
    "EMAIL;INTERNET:alex@example.com\r\n"
    "X-SOMEONE-ELSES:kept as it is\r\n"
    "NOTE:A long note that another program\r\n"
    " folded onto a second line\r\n"
    "X-LAST-USED:20261009\r\n"
    "END:VCARD\r\n";

/* As a modern address book writes one: version 3.0, groups, type=. */
static const char CARD_MODERN[] =
    "junk before the card\n"
    "BEGIN:VCARD\n"
    "VERSION:3.0\n"
    "\n"
    "N:Sample;Sam;;;\n"
    "FN:Sam Sample\n"
    "item1.TEL;type=CELL;type=pref:07700 900456\n"
    "item1.X-ABLabel:mobile\n"
    "UID:1234-ABCD\n"
    "END:VCARD\n"
    "BEGIN:VCARD\nFN:A second card\nEND:VCARD\n";

static void test_cards(void)
{
    static nl_vcard v, w;
    char out[256], small[4];
    static char big[NL_VCARD_MAX + 64];
    long n;
    int i;

    current = "card load";
    CHECK(nl_vcard_load(&v, CARD_CRLF, (long)strlen(CARD_CRLF)) == NL_VC_OK);
    CHECK(strchr(v.text, '\n') == NULL);                     /* CR only inside */
    CHECK(v.len == (long)strlen(v.text));
    CHECK(strncmp(v.text, "BEGIN:VCARD\rVERSION:2.1\r", 24) == 0);
    CHECK(strcmp(v.text + v.len - 10, "END:VCARD\r") == 0);
    CHECK(nl_vcard_load(&w, "FN:No card here\r", 16) == NL_VC_NOT_CARD);
    CHECK(nl_vcard_load(&w, "BEGIN:VCARD\rFN:Never ends\r", 26) == NL_VC_NOT_CARD);
    CHECK(nl_vcard_load(&w, "", 0) == NL_VC_NOT_CARD);
    memset(big, 'x', sizeof big);
    memcpy(big, "BEGIN:VCARD\rNOTE:", 17);
    memcpy(big + sizeof big - 11, "\rEND:VCARD\r", 11);
    CHECK(nl_vcard_load(&w, big, (long)sizeof big) == NL_VC_TOO_LONG);
    CHECK(w.len == 0);

    current = "card get";
    CHECK(nl_vcard_get(&v, "FN", 0, out, sizeof out) == 12 && strcmp(out, "Alex Example") == 0);
    CHECK(nl_vcard_get(&v, "fn", 0, out, sizeof out) == 12);             /* any case */
    CHECK(nl_vcard_get(&v, "F", 0, out, sizeof out) == NL_VC_MISSING);   /* not by its start */
    CHECK(nl_vcard_get(&v, "FNX", 0, out, sizeof out) == NL_VC_MISSING);
    CHECK(nl_vcard_count(&v, "TEL") == 2 && nl_vcard_count(&v, "FAX") == 0);
    CHECK(nl_vcard_get(&v, "TEL", 0, out, sizeof out) > 0 && strcmp(out, "+447700900123") == 0);
    CHECK(nl_vcard_get(&v, "TEL", 1, out, sizeof out) > 0 && strcmp(out, "01632 960123") == 0);
    CHECK(nl_vcard_get(&v, "TEL", 2, out, sizeof out) == NL_VC_MISSING);
    CHECK(nl_vcard_params(&v, "TEL", 0, out, sizeof out) == 10 && strcmp(out, "CELL;VOICE") == 0);
    CHECK(nl_vcard_has_param(out, "CELL") && nl_vcard_has_param(out, "voice"));
    CHECK(!nl_vcard_has_param(out, "HOME") && !nl_vcard_has_param(out, "CEL"));
    CHECK(nl_vcard_params(&v, "FN", 0, out, sizeof out) == 0 && out[0] == 0);
    CHECK(nl_vcard_get(&v, "NOTE", 0, out, sizeof out) > 0);
    CHECK(strcmp(out, "A long note that another programfolded onto a second line") == 0);
    CHECK(nl_vcard_get(&v, "X-LAST-USED", 0, out, sizeof out) == 8);
    CHECK(nl_date_parse(out, -1) == 20261009L);
    CHECK(nl_vcard_get(&v, "FN", 0, small, sizeof small) == NL_VC_TOO_LONG && small[0] == 0);
    CHECK(nl_vcard_get(&v, "X-KEPT-ON", 0, out, sizeof out) == NL_VC_MISSING);

    current = "card parts";
    CHECK(nl_vcard_get(&v, "N", 0, out, sizeof out) > 0);
    {
        char part[32];
        CHECK(nl_vcard_part(out, 0, part, sizeof part) == 7 && strcmp(part, "Example") == 0);
        CHECK(nl_vcard_part(out, 1, part, sizeof part) == 4 && strcmp(part, "Alex") == 0);
        CHECK(nl_vcard_part(out, 2, part, sizeof part) == 0 && part[0] == 0);
        CHECK(nl_vcard_part(out, 9, part, sizeof part) == 0);
        CHECK(nl_vcard_part("Smith\\;Jones;Pat", 0, part, sizeof part) == 11
              && strcmp(part, "Smith;Jones") == 0);
        CHECK(nl_vcard_part("Smith\\;Jones;Pat", 1, part, sizeof part) == 3
              && strcmp(part, "Pat") == 0);
        CHECK(nl_vcard_part("Example;Alex", 0, small, sizeof small) == NL_VC_TOO_LONG);
    }

    current = "card set";
    CHECK(nl_vcard_set(&v, "X-LAST-USED", "20261116") == NL_VC_OK);
    CHECK(nl_vcard_get(&v, "X-LAST-USED", 0, out, sizeof out) == 8 && strcmp(out, "20261116") == 0);
    CHECK(nl_vcard_set(&v, "X-KEPT-ON", "") == NL_VC_OK);    /* added, empty */
    CHECK(nl_vcard_get(&v, "X-KEPT-ON", 0, out, sizeof out) == 0);
    CHECK(nl_vcard_set(&v, "X-KEPT-ON", "20261201") == NL_VC_OK);
    CHECK(nl_vcard_get(&v, "X-KEPT-ON", 0, out, sizeof out) == 8);
    CHECK(nl_vcard_count(&v, "X-KEPT-ON") == 1);
    CHECK(nl_vcard_set(&v, "TEL", "+447700900999") == NL_VC_OK);        /* the first; parameters kept */
    CHECK(nl_vcard_params(&v, "TEL", 0, out, sizeof out) == 10 && strcmp(out, "CELL;VOICE") == 0);
    CHECK(nl_vcard_get(&v, "TEL", 0, out, sizeof out) > 0 && strcmp(out, "+447700900999") == 0);
    CHECK(nl_vcard_get(&v, "TEL", 1, out, sizeof out) > 0 && strcmp(out, "01632 960123") == 0);
    CHECK(nl_vcard_set(&v, "NOTE", "Short") == NL_VC_OK);    /* a folded value replaced whole */
    CHECK(nl_vcard_get(&v, "NOTE", 0, out, sizeof out) == 5 && strcmp(out, "Short") == 0);
    CHECK(strstr(v.text, "folded") == NULL);
    CHECK(nl_vcard_set(&v, "NOTE", "two\rlines") != NL_VC_OK);           /* refused */
    CHECK(nl_vcard_get(&v, "NOTE", 0, out, sizeof out) == 5);
    /* What NokiaLink doesn't know is untouched, and the card is still whole. */
    CHECK(strstr(v.text, "\rX-SOMEONE-ELSES:kept as it is\r") != NULL);
    CHECK(strcmp(v.text + v.len - 10, "END:VCARD\r") == 0);
    CHECK(v.len == (long)strlen(v.text));
    CHECK(nl_vcard_load(&w, v.text, v.len) == NL_VC_OK && w.len == v.len
          && memcmp(w.text, v.text, (size_t)v.len) == 0);    /* loads back the same */

    current = "card add and remove";
    CHECK(nl_vcard_add(&v, "TEL", "WORK", "01632 960777") == NL_VC_OK);
    CHECK(nl_vcard_count(&v, "TEL") == 3);
    CHECK(nl_vcard_get(&v, "TEL", 2, out, sizeof out) > 0 && strcmp(out, "01632 960777") == 0);
    CHECK(nl_vcard_params(&v, "TEL", 2, out, sizeof out) == 4 && strcmp(out, "WORK") == 0);
    CHECK(nl_vcard_remove(&v, "TEL", 1) == NL_VC_OK);
    CHECK(nl_vcard_count(&v, "TEL") == 2);
    CHECK(nl_vcard_get(&v, "TEL", 1, out, sizeof out) > 0 && strcmp(out, "01632 960777") == 0);
    CHECK(nl_vcard_remove(&v, "TEL", 5) == NL_VC_MISSING);
    CHECK(nl_vcard_remove(&v, "X-KEPT-ON", 0) == NL_VC_OK);
    CHECK(nl_vcard_count(&v, "X-KEPT-ON") == 0);
    CHECK(strcmp(v.text + v.len - 10, "END:VCARD\r") == 0);

    current = "card full";
    nl_vcard_init(&w);
    CHECK(strcmp(w.text, "BEGIN:VCARD\rVERSION:2.1\rEND:VCARD\r") == 0);
    CHECK(nl_vcard_set(&w, "FN", "Pat Sample") == NL_VC_OK);
    CHECK(strcmp(w.text, "BEGIN:VCARD\rVERSION:2.1\rFN:Pat Sample\rEND:VCARD\r") == 0);
    memset(out, 'n', 200);
    out[200] = 0;
    n = 0;
    for (i = 0; i < 20; i++) {
        long before = w.len;
        int r = nl_vcard_add(&w, "NOTE", 0, out);
        if (r == NL_VC_TOO_LONG) { n++; CHECK(w.len == before); }       /* refused whole */
        else CHECK(r == NL_VC_OK);
    }
    CHECK(n > 0 && w.len < NL_VCARD_MAX);
    CHECK(strcmp(w.text + w.len - 10, "END:VCARD\r") == 0);
    CHECK(nl_vcard_get(&w, "FN", 0, out, sizeof out) == 10);

    current = "card modern";
    CHECK(nl_vcard_load(&v, CARD_MODERN, (long)strlen(CARD_MODERN)) == NL_VC_OK);
    CHECK(nl_vcard_get(&v, "FN", 0, out, sizeof out) > 0 && strcmp(out, "Sam Sample") == 0);
    CHECK(nl_vcard_count(&v, "FN") == 1);                    /* only the first card */
    CHECK(nl_vcard_get(&v, "TEL", 0, out, sizeof out) > 0 && strcmp(out, "07700 900456") == 0);
    CHECK(nl_vcard_params(&v, "TEL", 0, out, sizeof out) > 0);
    CHECK(nl_vcard_has_param(out, "CELL") && nl_vcard_has_param(out, "pref")
          && !nl_vcard_has_param(out, "type"));
    CHECK(nl_vcard_get(&v, "X-ABLabel", 0, out, sizeof out) == 6);
    CHECK(nl_vcard_set(&v, "X-LAST-USED", "20261116") == NL_VC_OK);
    CHECK(strstr(v.text, "\ritem1.TEL;type=CELL;type=pref:07700 900456\r") != NULL);
    CHECK(strstr(v.text, "\rUID:1234-ABCD\rX-LAST-USED:20261116\rEND:VCARD\r") != NULL);
    CHECK(nl_number_same(strstr(v.text, "07700"), "+447700900456") == 0);  /* the rest of the card follows */
    CHECK(nl_vcard_get(&v, "TEL", 0, out, sizeof out) > 0
          && nl_number_same(out, "+447700900456"));
}

int main(void)
{
    test_dates();
    test_numbers();
    test_usage();
    test_clocks();
    test_cards();
    printf("store: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

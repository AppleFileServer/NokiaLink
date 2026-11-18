/* Host tests for building a vCalendar from a phone note (src/store/nlcalvcs).
 * Run by tests/run-host.sh. Data is made up. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nlcalvcs.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static void test_meeting(void)
{
    nl_cal_note nn; char out[512];
    memset(&nn, 0, sizeof nn);
    nn.type = NL_CAL_MEETING; nn.date = nl_date_make(2026,11,13);
    nn.hour = 17; nn.minute = 40; nn.has_alarm = 1; nn.alarm_before = 10;
    strcpy(nn.text, "Project review");
    current = "meeting";
    CHECK(nl_calvcs_build(&nn, "nokialink-cal-1", out, sizeof out) > 0);
    CHECK(strstr(out, "BEGIN:VCALENDAR\rVERSION:1.0\rBEGIN:VEVENT\r") != 0);
    CHECK(strstr(out, "SUMMARY:Project review\r") != 0);
    CHECK(strstr(out, "DTSTART:20261113T174000\r") != 0);
    CHECK(strstr(out, "X-NOKIA-ALARM-BEFORE:10\r") != 0);
    CHECK(strstr(out, "CATEGORIES:MEETING\r") != 0);
    CHECK(strstr(out, "UID:nokialink-cal-1\r") != 0);
    CHECK(strstr(out, "END:VEVENT\rEND:VCALENDAR\r") != 0);
}

static void test_reminder(void)
{
    nl_cal_note nn; char out[512];
    memset(&nn, 0, sizeof nn);
    nn.type = NL_CAL_REMINDER; nn.date = nl_date_make(2026,11,17);
    strcpy(nn.text, "Breakfast");
    current = "reminder (date only)";
    CHECK(nl_calvcs_build(&nn, "u2", out, sizeof out) > 0);
    CHECK(strstr(out, "DTSTART:20261117\r") != 0);     /* no time */
    CHECK(strstr(out, "CATEGORIES:REMINDER\r") != 0);
    CHECK(strstr(out, "X-NOKIA-ALARM-BEFORE") == 0);   /* no alarm */
}

static void test_call(void)
{
    nl_cal_note nn; char out[512];
    memset(&nn, 0, sizeof nn);
    nn.type = NL_CAL_CALL; nn.date = nl_date_make(2026,11,20);
    nn.hour = 9; nn.minute = 5; strcpy(nn.text, "Ring plumber");
    strcpy(nn.number, "+447700900050");
    current = "call";
    CHECK(nl_calvcs_build(&nn, "u3", out, sizeof out) > 0);
    CHECK(strstr(out, "DTSTART:20261120T090500\r") != 0);
    CHECK(strstr(out, "DESCRIPTION:+447700900050\r") != 0);
    CHECK(strstr(out, "CATEGORIES:CALL\r") != 0);
}

static void test_birthday(void)
{
    nl_cal_note nn; char out[512];
    memset(&nn, 0, sizeof nn);
    nn.type = NL_CAL_BIRTHDAY; nn.date = nl_date_make(2000,3,14);
    nn.birth_year = 1988; strcpy(nn.text, "Grace");
    current = "birthday (yearly)";
    CHECK(nl_calvcs_build(&nn, "u4", out, sizeof out) > 0);
    CHECK(strstr(out, "DTSTART:19880314\r") != 0);     /* birth year, month/day */
    CHECK(strstr(out, "RRULE:YM1\r") != 0);
    CHECK(strstr(out, "X-NOKIA-BIRTH-YEAR:1988\r") != 0);
    CHECK(strstr(out, "CATEGORIES:BIRTHDAY\r") != 0);
}

static void test_space(void)
{
    nl_cal_note nn; char out[16];
    memset(&nn, 0, sizeof nn);
    nn.type = NL_CAL_REMINDER; strcpy(nn.text, "x");
    current = "out of space";
    CHECK(nl_calvcs_build(&nn, "u5", out, sizeof out) == NL_CALVCS_ERR_SPACE);
}

int main(void)
{
    test_meeting();
    test_reminder();
    test_call();
    test_birthday();
    test_space();
    printf("calvcs: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

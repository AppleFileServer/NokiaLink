/* Host tests for the call-log journal (src/store/nllogarc).
 * Run by tests/run-host.sh. All data is made up. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nllogarc.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static nl_log_rec rec(void)
{
    nl_log_rec r;
    r.kind = "received"; r.number = "+447700900123"; r.name = "Ada";
    r.date = nl_date_make(2026, 11, 18); r.secs = 9L*3600 + 24*60 + 5; r.has_time = 1;
    return r;
}

static void test_format(void)
{
    nl_log_rec r = rec();
    char out[256]; long n;
    current = "format";
    n = nl_logarc_format(&r, out, sizeof out);
    CHECK(n > 0 && out[n-1] == '\n' && out[8] == '\t');
    CHECK(strstr(out, "\treceived\t+447700900123\tAda\t2026-11-18 09:24:05\n") != 0);
}

static void test_no_time(void)
{
    nl_log_rec r = rec();
    char out[256];
    current = "no time";
    r.has_time = 0; r.date = 0; r.name = "";
    CHECK(nl_logarc_format(&r, out, sizeof out) > 0);
    CHECK(strstr(out, "\treceived\t+447700900123\t\t\n") != 0);   /* empty name + empty date */
}

static void test_fingerprint(void)
{
    nl_log_rec a = rec(), b = rec();
    current = "stable";
    CHECK(nl_logarc_fingerprint(&a) == nl_logarc_fingerprint(&b));
    current = "name ignored";
    b.name = "Someone"; CHECK(nl_logarc_fingerprint(&a) == nl_logarc_fingerprint(&b));
    current = "kind matters";
    b = rec(); b.kind = "dialled"; CHECK(nl_logarc_fingerprint(&a) != nl_logarc_fingerprint(&b));
    current = "time matters";
    b = rec(); b.secs += 1; CHECK(nl_logarc_fingerprint(&a) != nl_logarc_fingerprint(&b));
    current = "number matters";
    b = rec(); b.number = "+447700900124"; CHECK(nl_logarc_fingerprint(&a) != nl_logarc_fingerprint(&b));
}

static void test_space(void)
{
    nl_log_rec r = rec();
    char out[8];
    current = "out of space";
    CHECK(nl_logarc_format(&r, out, sizeof out) == NL_LOGARC_ERR_SPACE);
}

int main(void)
{
    test_format();
    test_no_time();
    test_fingerprint();
    test_space();
    printf("logarc: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

/* Host tests for the message archive journal (src/store/nlmsgarc).
 * Run by tests/run-host.sh. All data here is made up. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nlmsgarc.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static nl_msg_rec rec(void)
{
    nl_msg_rec r;
    r.folder = "Inbox"; r.incoming = 1; r.number = "+447700900123";
    r.name = "Ada"; r.date = nl_date_make(2026, 11, 16); r.secs = 17L*3600 + 1*60;
    r.has_time = 1; r.parts = 1; r.text = "Hello there";
    return r;
}

static void test_format(void)
{
    nl_msg_rec r = rec();
    char out[256];
    long n;
    current = "format";
    n = nl_msgarc_format(&r, out, sizeof out);
    CHECK(n > 0);
    /* columns: fp \t folder \t direction \t number \t name \t date \t parts \t text \n */
    CHECK(strstr(out, "\tInbox\treceived\t+447700900123\tAda\t2026-11-16 17:01\t1\tHello there\n") != 0);
    CHECK(out[n-1] == '\n' && out[8] == '\t');   /* 8 hex fingerprint then a tab */
}

static void test_escaping(void)
{
    nl_msg_rec r = rec();
    char out[256];
    current = "escaping";
    r.text = "a\tb\nc\\d";
    CHECK(nl_msgarc_format(&r, out, sizeof out) > 0);
    CHECK(strstr(out, "\ta\\tb\\nc\\\\d\n") != 0);   /* tab/newline/backslash escaped */
}

static void test_no_time(void)
{
    nl_msg_rec r = rec();
    char out[256];
    current = "sent, no time";
    r.incoming = 0; r.folder = "Sent"; r.has_time = 0; r.date = 0;
    CHECK(nl_msgarc_format(&r, out, sizeof out) > 0);
    CHECK(strstr(out, "\tSent\tsent\t") != 0);
    CHECK(strstr(out, "\t\t1\t") != 0);            /* empty date column */
}

static void test_fingerprint(void)
{
    nl_msg_rec a = rec(), b = rec();
    current = "fingerprint stable";
    CHECK(nl_msgarc_fingerprint(&a) == nl_msgarc_fingerprint(&b));
    current = "name/parts do not change fingerprint";
    b.name = "Someone Else"; b.parts = 4;
    CHECK(nl_msgarc_fingerprint(&a) == nl_msgarc_fingerprint(&b));
    current = "different text changes fingerprint";
    b = rec(); b.text = "Hello there!";
    CHECK(nl_msgarc_fingerprint(&a) != nl_msgarc_fingerprint(&b));
    current = "different number changes fingerprint";
    b = rec(); b.number = "+447700900124";
    CHECK(nl_msgarc_fingerprint(&a) != nl_msgarc_fingerprint(&b));
    current = "different date changes fingerprint";
    b = rec(); b.date = nl_date_make(2026, 11, 17);
    CHECK(nl_msgarc_fingerprint(&a) != nl_msgarc_fingerprint(&b));
}

static void test_space(void)
{
    nl_msg_rec r = rec();
    char out[8];
    current = "out of space";
    CHECK(nl_msgarc_format(&r, out, sizeof out) == NL_MSGARC_ERR_SPACE);
}

int main(void)
{
    test_format();
    test_escaping();
    test_no_time();
    test_fingerprint();
    test_space();
    printf("msgarc: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

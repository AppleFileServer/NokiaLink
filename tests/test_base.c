/* Host tests for the last-sync base (src/core/nlbase).
 * Run by tests/run-host.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlbase.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static nl_base b, b2;

static void test_add_find(void)
{
    nl_u32 h;
    nl_base_clear(&b);
    current = "add and find";
    CHECK(nl_base_add(&b, "k1", 0x11111111) == NL_BASE_OK);
    CHECK(nl_base_add(&b, "k2", 0x22222222) == NL_BASE_OK);
    CHECK(b.n == 2);
    CHECK(nl_base_find(&b, "k1", &h) && h == 0x11111111);
    CHECK(nl_base_find(&b, "k2", &h) && h == 0x22222222);
    CHECK(!nl_base_find(&b, "k3", &h));
    current = "add replaces";
    CHECK(nl_base_add(&b, "k1", 0x99999999) == NL_BASE_OK && b.n == 2);
    CHECK(nl_base_find(&b, "k1", &h) && h == 0x99999999);
}

static void test_roundtrip(void)
{
    char buf[256]; long n; int i; nl_u32 h;
    nl_base_clear(&b);
    nl_base_add(&b, "ABC-1", 0xDEADBEEF);
    nl_base_add(&b, "ABC-2", 0x00000001);
    nl_base_add(&b, "ABC-3", 0xCAFEBABE);
    current = "write";
    n = nl_base_write(&b, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(strncmp(buf, "NokiaLink base 1\r", 17) == 0);
    CHECK(strstr(buf, "ABC-1\tDEADBEEF\r") != NULL);

    current = "parse back (round-trip)";
    CHECK(nl_base_parse(&b2, buf, n) == NL_BASE_OK);
    CHECK(b2.n == 3);
    for (i = 0; i < 3; i++) {
        CHECK(nl_base_find(&b2, b.key[i], &h) && h == b.hash[i]);
    }

    current = "write buffer too small";
    CHECK(nl_base_write(&b, buf, 10) == NL_BASE_ERR_SPACE);
}

static void test_lineends_and_format(void)
{
    const char *lf = "NokiaLink base 1\nx\t0000000A\ny\t00000014\n";
    const char *crlf = "NokiaLink base 1\r\nx\t0000000A\r\n";
    nl_u32 h;
    current = "parse LF";
    CHECK(nl_base_parse(&b2, lf, (long)strlen(lf)) == NL_BASE_OK && b2.n == 2);
    CHECK(nl_base_find(&b2, "x", &h) && h == 10 && nl_base_find(&b2, "y", &h) && h == 20);
    current = "parse CRLF";
    CHECK(nl_base_parse(&b2, crlf, (long)strlen(crlf)) == NL_BASE_OK && b2.n == 1);
    current = "bad format";
    CHECK(nl_base_parse(&b2, "not a base\nx\t1\n", 15) == NL_BASE_ERR_FORMAT);
    CHECK(nl_base_parse(&b2, "", 0) == NL_BASE_ERR_FORMAT);
    current = "header only";
    CHECK(nl_base_parse(&b2, "NokiaLink base 1\r", 17) == NL_BASE_OK && b2.n == 0);
}

static void test_feeds_compare(void)
{
    nl_sync_item base_items[NL_BASE_MAX];
    int nb;
    nl_sync_item phone[] = { {"k1",0xA}, {"k2",0xB} };   /* k2 changed on phone */
    nl_sync_item store[] = { {"k1",0xA}, {"k2",0xA} };
    nl_sync_action out[8]; int nout;
    nl_base_clear(&b);
    nl_base_add(&b, "k1", 0xA);
    nl_base_add(&b, "k2", 0xA);
    nl_base_items(&b, base_items, NL_BASE_MAX, &nb);
    current = "base feeds three-way compare";
    CHECK(nb == 2);
    CHECK(nl_sync_compare(base_items, nb, phone, 2, store, 2, out, 8, &nout) == NL_SYNC_OK);
    CHECK(nout == 1 && out[0].verb == NL_SYNC_UPDATE_STORE && strcmp(out[0].key,"k2")==0);
}

int main(void)
{
    test_add_find();
    test_roundtrip();
    test_lineends_and_format();
    test_feeds_compare();
    printf("base: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

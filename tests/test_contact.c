/* Host tests for CRC-32 and the contact sync identity
 * (src/store/nlcrc, nlcontact). Run by tests/run-host.sh. Made-up data. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nlcrc.h"
#include "../src/store/nlcontact.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static void test_crc(void)
{
    current = "crc32 check value";
    CHECK(nl_crc32("123456789", 9) == 0xCBF43926UL);   /* standard CRC-32 check */
    current = "crc32 empty";
    CHECK(nl_crc32("", 0) == 0x00000000UL);
    current = "crc32 pieces == whole";
    { nl_u32 c = nl_crc32_begin();
      c = nl_crc32_add(c, "123", 3); c = nl_crc32_add(c, "456789", 6);
      CHECK(nl_crc32_end(c) == 0xCBF43926UL); }
}

static nl_u32 hash_of(const char *text)
{
    static nl_vcard v;
    nl_vcard_load(&v, text, (long)strlen(text));
    return nl_contact_hash(&v);
}

static void test_key(void)
{
    static nl_vcard v;
    char key[64];
    current = "key from UID";
    nl_vcard_load(&v, "BEGIN:VCARD\nVERSION:2.1\nFN:Pat\nUID:ABC-123\nEND:VCARD\n", 52);
    CHECK(nl_contact_key(&v, key, sizeof key) > 0 && strcmp(key,"ABC-123")==0);
    current = "key falls back to FN";
    nl_vcard_load(&v, "BEGIN:VCARD\nVERSION:2.1\nFN:Pat Sample\nEND:VCARD\n", 47);
    CHECK(nl_contact_key(&v, key, sizeof key) > 0 && strcmp(key,"Pat Sample")==0);
}

static void test_hash(void)
{
    const char *A =
        "BEGIN:VCARD\nVERSION:2.1\nN:Sample;Pat\nFN:Pat Sample\n"
        "TEL;CELL:+447700900123\nTEL;HOME:01632 960111\n"
        "EMAIL:Pat@Example.cm\nX-LAST-USED:20260101\nUID:ABC\nEND:VCARD\n";
    /* same content: lines reordered, number re-punctuated, email case,
       different last-used date */
    const char *B =
        "BEGIN:VCARD\nVERSION:2.1\nEMAIL:pat@example.cm\nUID:ABC\n"
        "TEL;HOME:01632960111\nFN:Pat Sample\nTEL;CELL:07700 900123\n"
        "N:Sample;Pat\nX-LAST-USED:20270505\nEND:VCARD\n";
    /* a real change: different mobile number */
    const char *C =
        "BEGIN:VCARD\nVERSION:2.1\nN:Sample;Pat\nFN:Pat Sample\n"
        "TEL;CELL:+447700900999\nTEL;HOME:01632 960111\n"
        "EMAIL:Pat@Example.cm\nUID:ABC\nEND:VCARD\n";
    /* a real change: different name */
    const char *D =
        "BEGIN:VCARD\nVERSION:2.1\nN:Sample;Pat\nFN:Pat SAMPLE\n"
        "TEL;CELL:+447700900123\nTEL;HOME:01632 960111\n"
        "EMAIL:Pat@Example.cm\nUID:ABC\nEND:VCARD\n";

    current = "hash ignores order, format, case, dates";
    CHECK(hash_of(A) == hash_of(B));
    current = "hash changes with a different number";
    CHECK(hash_of(A) != hash_of(C));
    current = "hash changes with a different name";
    CHECK(hash_of(A) != hash_of(D));
    current = "hash is stable (same input twice)";
    CHECK(hash_of(A) == hash_of(A));
}

int main(void)
{
    test_crc();
    test_key();
    test_hash();
    printf("contact: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

/* Host tests for the UCS-2 <-> Mac Roman codec (src/store/nltext).
 * Run by tests/run-host.sh. All text here is made up. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nltext.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

/* Build a big-endian UCS-2 buffer from a list of code points. */
static long mk(const unsigned *cps, long n, nl_u8 *out)
{
    long i;
    for (i = 0; i < n; i++) { out[2*i] = (nl_u8)(cps[i] >> 8); out[2*i+1] = (nl_u8)cps[i]; }
    return n;
}

static void test_to_macroman(void)
{
    nl_u8 u[64];
    char out[64];
    long lost, n;

    current = "ucs2->mac ascii";
    { const unsigned c[] = {'H','e','l','l','o'}; long nc = mk(c,5,u);
      n = nl_ucs2_to_macroman(u, nc, out, sizeof out, &lost);
      CHECK(n == 5 && strcmp(out, "Hello") == 0 && lost == 0); }

    current = "ucs2->mac high chars";
    /* é (U+00E9), £ (U+00A3), © (U+00A9) -> Mac Roman 0x8E, 0xA3, 0xA9 */
    { const unsigned c[] = {0x00E9, 0x00A3, 0x00A9}; long nc = mk(c,3,u);
      n = nl_ucs2_to_macroman(u, nc, out, sizeof out, &lost);
      CHECK(n == 3 && lost == 0);
      CHECK((nl_u8)out[0] == 0x8E && (nl_u8)out[1] == 0xA3 && (nl_u8)out[2] == 0xA9); }

    current = "ucs2->mac unmappable";
    /* Cyrillic (U+0422 U+0430) has no Mac Roman form -> '?' each, counted. */
    { const unsigned c[] = {'A', 0x0422, 0x0430, 'Z'}; long nc = mk(c,4,u);
      n = nl_ucs2_to_macroman(u, nc, out, sizeof out, &lost);
      CHECK(n == 4 && lost == 2 && strcmp(out, "A??Z") == 0); }

    current = "ucs2->mac emoji (surrogate pair)";
    /* U+1F4A9 = high D83D, low DCA9: one character, no Mac Roman form. */
    { const unsigned c[] = {'x', 0xD83D, 0xDCA9, 'y'}; long nc = mk(c,4,u);
      n = nl_ucs2_to_macroman(u, nc, out, sizeof out, &lost);
      CHECK(n == 3 && lost == 1 && strcmp(out, "x?y") == 0); }

    current = "ucs2->mac lone high surrogate";
    { const unsigned c[] = {'a', 0xD83D}; long nc = mk(c,2,u);
      n = nl_ucs2_to_macroman(u, nc, out, sizeof out, &lost);
      CHECK(n == 2 && lost == 1 && strcmp(out, "a?") == 0); }

    current = "ucs2->mac out too small";
    { const unsigned c[] = {'a','b','c','d'}; long nc = mk(c,4,u);
      n = nl_ucs2_to_macroman(u, nc, out, 3, &lost);     /* room for 2 + zero */
      CHECK(n == -1 && out[0] == 0); }

    current = "ucs2->mac null lost arg";
    { const unsigned c[] = {'o','k'}; long nc = mk(c,2,u);
      n = nl_ucs2_to_macroman(u, nc, out, sizeof out, 0);
      CHECK(n == 2 && strcmp(out, "ok") == 0); }
}

static void test_to_ucs2(void)
{
    nl_u16 w[64];
    long n;

    current = "mac->ucs2";
    { char in[4]; in[0]='A'; in[1]=(char)0x8E; in[2]=(char)0xA3; in[3]=(char)0xA9;
      n = nl_macroman_to_ucs2(in, 4, w, 64);
      CHECK(n == 4);
      CHECK(w[0]=='A' && w[1]==0x00E9 && w[2]==0x00A3 && w[3]==0x00A9); }

    current = "mac->ucs2 out too small";
    { char in[4]; memcpy(in, "abcd", 4); n = nl_macroman_to_ucs2(in, 4, w, 3); CHECK(n == -1); }

    current = "macroman_char";
    CHECK(nl_macroman_char('A') == 'A');
    CHECK(nl_macroman_char(0x8E) == 0x00E9);   /* e-acute */
    CHECK(nl_macroman_char(0xA3) == 0x00A3);   /* pound */
    CHECK(nl_macroman_char(0xDB) == 0x20AC);   /* euro */
}

static void test_roundtrip(void)
{
    /* Every Mac Roman byte -> UCS-2 -> Mac Roman returns the same byte. */
    current = "roundtrip all 256";
    int b, bad = 0;
    for (b = 0; b < 256; b++) {
        char in = (char)b, back[2];
        nl_u16 w[1];
        long lost;
        /* skip the control bytes the codec does not carry back as text */
        nl_u16 u = nl_macroman_char((nl_u8)b);
        (void)w;
        {
            nl_u8 be[2]; be[0]=(nl_u8)(u>>8); be[1]=(nl_u8)u;
            long n = nl_ucs2_to_macroman(be, 1, back, sizeof back, &lost);
            if (b >= 0x20 && b != 0x7F) {   /* printable range round-trips exactly */
                if (!(n == 1 && (nl_u8)back[0] == (nl_u8)b && lost == 0)) bad++;
            }
        }
        (void)in;
    }
    CHECK(bad == 0);
}

int main(void)
{
    test_to_macroman();
    test_to_ucs2();
    test_roundtrip();
    printf("text: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

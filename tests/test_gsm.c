/* Host tests for the GSM text/number/time codec (src/core/nlgsm).
 * Run by tests/run-host.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlgsm.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

/* An independent 7-bit packer, so the unpacker is checked by round-trip. */
static long pack7(const nl_u8 *s, int n, int fill, nl_u8 *out, int outmax)
{
    int total = fill + n * 7, nbytes = (total + 7) / 8, i;
    for (i = 0; i < nbytes && i < outmax; i++) out[i] = 0;
    for (i = 0; i < n; i++) {
        int bit = fill + i * 7, byte = bit >> 3, shift = bit & 7;
        out[byte] |= (nl_u8)((s[i] << shift) & 0xFF);
        if (shift > 1 && byte + 1 < outmax) out[byte + 1] |= (nl_u8)(s[i] >> (8 - shift));
    }
    return nbytes;
}

static void test_unpack_roundtrip(void)
{
    nl_u8 sept[16], packed[16], back[16];
    int i, n;
    current = "unpack round-trip";
    /* 'H','e','l','l','o' are the same codes in ASCII and GSM default */
    { const char *s = "Hello"; n = (int)strlen(s);
      for (i = 0; i < n; i++) sept[i] = (nl_u8)s[i];
      long nb = pack7(sept, n, 0, packed, sizeof packed);
      int got = nl_gsm7_unpack(packed, nb, 0, n, back, sizeof back);
      CHECK(got == n);
      CHECK(memcmp(back, sept, n) == 0); }

    current = "unpack with fill bits";
    /* a UDH often leaves a 1-bit gap; round-trip with fill=1 */
    { nl_u8 s2[3] = {0x41, 0x42, 0x43}; long nb = pack7(s2, 3, 1, packed, sizeof packed);
      int got = nl_gsm7_unpack(packed, nb, 1, 3, back, sizeof back);
      CHECK(got == 3 && back[0]==0x41 && back[1]==0x42 && back[2]==0x43); }
}

static void test_alphabet(void)
{
    nl_u8 sept[8]; nl_u16 uni[8]; long n;
    current = "default alphabet to ucs2";
    sept[0]=0x00; sept[1]=0x01; sept[2]=0x02; sept[3]=0x15;  /* @ £ $ Omega */
    n = nl_gsm7_to_ucs2(sept, 4, uni, 8);
    CHECK(n==4 && uni[0]==0x0040 && uni[1]==0x00A3 && uni[2]==0x0024 && uni[3]==0x03A9);

    current = "escape extension euro";
    sept[0]=NL_GSM_ESC; sept[1]=0x65;
    n = nl_gsm7_to_ucs2(sept, 2, uni, 8);
    CHECK(n==1 && uni[0]==0x20AC);

    current = "escape extension brace";
    sept[0]=NL_GSM_ESC; sept[1]=0x28;
    n = nl_gsm7_to_ucs2(sept, 2, uni, 8);
    CHECK(n==1 && uni[0]=='{');
}

static void test_to_macroman(void)
{
    nl_u8 sept[16], packed[16];
    char out[32]; long lost, got; int i, n;
    current = "gsm7 to mac roman (plain)";
    { const char *s = "Hi there"; n=(int)strlen(s);
      for (i=0;i<n;i++) sept[i]=(nl_u8)s[i];
      long nb = pack7(sept,n,0,packed,sizeof packed);
      got = nl_gsm7_to_macroman(packed, nb, 0, n, out, sizeof out, &lost);
      CHECK(got==n && strcmp(out,"Hi there")==0 && lost==0); }

    current = "gsm7 pound sign maps to mac roman";
    { sept[0]='5'; sept[1]=0x01; sept[2]='0';   /* "5 £ 0" -> 5,pound,0 */
      long nb = pack7(sept,3,0,packed,sizeof packed);
      got = nl_gsm7_to_macroman(packed, nb, 0, 3, out, sizeof out, &lost);
      CHECK(got==3 && (nl_u8)out[1]==0xA3 && lost==0); }

    current = "gsm7 unmappable greek phi -> ?";
    { sept[0]='A'; sept[1]=0x12; sept[2]='Z';   /* Phi has no Mac Roman form */
      long nb = pack7(sept,3,0,packed,sizeof packed);
      got = nl_gsm7_to_macroman(packed, nb, 0, 3, out, sizeof out, &lost);
      CHECK(got==3 && strcmp(out,"A?Z")==0 && lost==1); }
}

static void test_number(void)
{
    nl_u8 p[16]; char out[32]; long n;
    current = "international number";
    /* +447700900123 : 12 digits, TOA 0x91, semi-octets */
    p[0]=12; p[1]=0x91;
    p[2]=0x44; p[3]=0x77; p[4]=0x00; p[5]=0x09; p[6]=0x10; p[7]=0x32;
    n = nl_gsm_number(p, 8, 1, out, sizeof out);
    CHECK(n==13 && strcmp(out,"+447700900123")==0);

    current = "national number";
    p[1]=0x81;                              /* unknown/national: no + */
    n = nl_gsm_number(p, 8, 1, out, sizeof out);
    CHECK(n==12 && strcmp(out,"447700900123")==0);

    current = "odd length with filler";
    /* "12345": pairs (1,2)(3,4)(5,F) */
    p[0]=5; p[1]=0x81; p[2]=0x21; p[3]=0x43; p[4]=0xF5;
    n = nl_gsm_number(p, 5, 1, out, sizeof out);
    CHECK(n==5 && strcmp(out,"12345")==0);

    current = "bad toa";
    p[1]=0x11;                              /* top bit clear -> invalid */
    CHECK(nl_gsm_number(p, 5, 1, out, sizeof out) == NL_GSM_ERR_BADNUM);

    current = "short";
    CHECK(nl_gsm_number(p, 1, 1, out, sizeof out) == NL_GSM_ERR_SHORT);
}

static void test_timestamp(void)
{
    nl_u8 p[7]; nl_stamp st; int tz, r;
    current = "timestamp 2026-11-17 14:30:45 +1h";
    /* swapped BCD: 26,11,17,14,30,45 ; tz = +4 quarters */
    p[0]=0x62; p[1]=0x11; p[2]=0x71; p[3]=0x41; p[4]=0x03; p[5]=0x54; p[6]=0x40;
    r = nl_gsm_timestamp(p, 7, &st, &tz);
    CHECK(r==NL_GSM_OK);
    CHECK(st.date==nl_date_make(2026,11,17));
    CHECK(st.secs==14L*3600+30*60+45);
    CHECK(tz==4);

    current = "timestamp negative tz";
    p[6]=0x48;                              /* value 4, sign bit set -> -4 */
    r = nl_gsm_timestamp(p, 7, &st, &tz);
    CHECK(r==NL_GSM_OK && tz==-4);

    current = "timestamp short";
    CHECK(nl_gsm_timestamp(p, 5, &st, &tz) == NL_GSM_ERR_SHORT);
}

int main(void)
{
    test_unpack_roundtrip();
    test_alphabet();
    test_to_macroman();
    test_number();
    test_timestamp();
    printf("gsm: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

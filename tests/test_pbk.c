/* Host tests for the contacts phone-service (src/core/nlpbk).
 * Run by tests/run-host.sh. All contacts here are made up. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlpbk.h"

/* Sub-block field ids, as the phone sends them (private to nlpbk.c; the
 * test builds raw frames so it keeps its own copies). */
#define PBK_NAME   0x07
#define PBK_EMAIL  0x08
#define PBK_POSTAL 0x09
#define PBK_NOTE   0x0A

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

/* write UCS-2 big-endian for an ASCII string; return bytes */
static int ucs2(nl_u8 *p, const char *s)
{ int n=0; for (; *s; s++){ p[n++]=0x00; p[n++]=(nl_u8)*s; } return n; }

/* append a text sub-block (id, text); return new offset */
static long text_block(nl_u8 *b, long off, nl_u8 id, const char *s)
{
    int tb = 2 * (int)strlen(s);
    long bs = 6 + tb;
    b[off] = id; b[off+1] = 0; b[off+2] = (nl_u8)(bs>>8); b[off+3] = (nl_u8)bs;
    b[off+4] = 0; b[off+5] = (nl_u8)tb;
    ucs2(b + off + 6, s);
    return off + bs;
}

/* append a number sub-block (type, number); return new offset */
static long num_block(nl_u8 *b, long off, nl_u8 ntype, const char *s)
{
    int tb = 2 * (int)strlen(s);
    long bs = 10 + tb;
    b[off] = 0x0B; b[off+1] = 0; b[off+2] = (nl_u8)(bs>>8); b[off+3] = (nl_u8)bs;
    b[off+4] = 0; b[off+5] = ntype; b[off+6] = 0; b[off+7] = 0; b[off+8] = 0;
    b[off+9] = (nl_u8)tb;
    ucs2(b + off + 10, s);
    return off + bs;
}

static void test_requests(void)
{
    nl_u8 out[20]; long n;
    current = "req read ME";
    n = nl_pbk_req_read(out, sizeof out, NL_PBK_MEM_ME, 0x0005);
    CHECK(n == 14);
    CHECK(out[0]==0 && out[1]==1 && out[2]==1 && out[3]==0x07 && out[9]==0x05
          && out[10]==0x00 && out[11]==0x05);
    current = "req read SIM";
    n = nl_pbk_req_read(out, sizeof out, NL_PBK_MEM_SIM, 0x0102);
    CHECK(n == 14 && out[9]==0x06 && out[10]==0x01 && out[11]==0x02);
    current = "req bad memory";
    CHECK(nl_pbk_req_read(out, sizeof out, 0x09, 1) == NL_PBK_ERR_MEM);
    current = "req status";
    n = nl_pbk_req_status(out, sizeof out, NL_PBK_MEM_ME);
    CHECK(n == 6 && out[3]==0x03 && out[4]==0x02 && out[5]==0x05);
    current = "req no space";
    CHECK(nl_pbk_req_read(out, 10, NL_PBK_MEM_ME, 1) == NL_PBK_ERR_SPACE);
}

static void test_status(void)
{
    nl_u8 b[24]; nl_u16 used, total; int r;
    memset(b, 0, sizeof b);
    current = "status 2-byte capacity";
    b[10] = 0x10; b[14] = (500>>8); b[15] = (500&0xFF); b[16] = 0; b[17] = 51;
    r = nl_pbk_parse_status(b, 20, &used, &total);
    CHECK(r==NL_PBK_OK && used==51 && total==500);
    current = "status 1-byte capacity";
    b[10] = 0x00; b[18] = 250; b[16] = 0; b[17] = 10;
    r = nl_pbk_parse_status(b, 20, &used, &total);
    CHECK(r==NL_PBK_OK && used==10 && total==250);
    current = "status short";
    CHECK(nl_pbk_parse_status(b, 12, &used, &total) == NL_PBK_ERR_SHORT);
}

static void test_entry(void)
{
    nl_u8 b[256]; long off; nl_pbk_entry e; int r;
    memset(b, 0, sizeof b);
    b[6] = 0x01;                 /* not an error subframe */
    off = 18;
    off = text_block(b, off, PBK_NAME, "Pat Sample");
    off = num_block(b, off, 0x03 /*mobile*/, "07700900123");
    off = num_block(b, off, 0x02 /*home*/, "01632960111");
    off = text_block(b, off, PBK_EMAIL, "pat@example.cm");
    off = text_block(b, off, PBK_NOTE, "A friend");

    current = "entry decode";
    r = nl_pbk_parse_entry(b, off, 7, &e);
    CHECK(r == NL_PBK_OK && e.location == 7);
    CHECK(strcmp(e.name, "Pat Sample") == 0 && e.name_lost == 0);
    CHECK(e.nnumbers == 2);
    CHECK(e.numbers[0].type == NL_NUM_MOBILE && strcmp(e.numbers[0].text, "07700900123") == 0);
    CHECK(e.numbers[1].type == NL_NUM_HOME && strcmp(e.numbers[1].text, "01632960111") == 0);
    CHECK(strcmp(e.email, "pat@example.cm") == 0);
    CHECK(strcmp(e.note, "A friend") == 0);
    CHECK(e.postal[0] == 0);

    current = "entry empty slot";
    { nl_u8 x[20]; memset(x,0,sizeof x); x[6] = 0x0F;
      CHECK(nl_pbk_parse_entry(x, 20, 9, &e) == NL_PBK_ERR_EMPTY); }

    current = "entry short";
    { nl_u8 x[8]; memset(x,0,sizeof x);
      CHECK(nl_pbk_parse_entry(x, 8, 1, &e) == NL_PBK_ERR_SHORT); }
}

static void test_sim_and_general(void)
{
    nl_u8 b[128]; long off; nl_pbk_entry e;
    memset(b, 0, sizeof b); b[6]=0x01; off=18;
    off = text_block(b, off, PBK_NAME, "Lebara");
    off = num_block(b, off, 0x00 /*general*/, "38885");
    current = "SIM-style entry (name + one general number)";
    CHECK(nl_pbk_parse_entry(b, off, 3, &e) == NL_PBK_OK);
    CHECK(strcmp(e.name,"Lebara")==0 && e.nnumbers==1);
    CHECK(e.numbers[0].type==NL_NUM_GENERAL && strcmp(e.numbers[0].text,"38885")==0);
}

static void test_lost_and_cap(void)
{
    nl_u8 b[512]; long off; nl_pbk_entry e; int i;
    memset(b, 0, sizeof b); b[6]=0x01; off=18;
    /* name with a non-Mac-Roman char: 'A', U+0416, 'Z' */
    b[off]=PBK_NAME; b[off+2]=0; b[off+3]=12; b[off+5]=6;
    b[off+6]=0x00; b[off+7]='A'; b[off+8]=0x04; b[off+9]=0x16; b[off+10]=0x00; b[off+11]='Z';
    off += 12;
    current = "name lost char counted";
    /* add more than NL_PBK_MAX_NUMBERS numbers */
    for (i = 0; i < NL_PBK_MAX_NUMBERS + 3; i++) off = num_block(b, off, 0x03, "0770090000");
    CHECK(nl_pbk_parse_entry(b, off, 1, &e) == NL_PBK_OK);
    CHECK(strcmp(e.name, "A?Z") == 0 && e.name_lost == 1);
    CHECK(e.nnumbers == NL_PBK_MAX_NUMBERS);   /* extra numbers dropped, not overflowed */
}

/* a DATETIME sub-block (id 0x13): date d and seconds-of-day */
static long dt_block(nl_u8 *b, long off, int y, int mo, int da, int hh, int mi, int se)
{
    long bs = 6 + 8;
    b[off]=0x13; b[off+1]=0; b[off+2]=(nl_u8)(bs>>8); b[off+3]=(nl_u8)bs; b[off+4]=0; b[off+5]=8;
    b[off+6]=(nl_u8)(y>>8); b[off+7]=(nl_u8)y; b[off+8]=(nl_u8)mo; b[off+9]=(nl_u8)da;
    b[off+10]=(nl_u8)hh; b[off+11]=(nl_u8)mi; b[off+12]=(nl_u8)se; b[off+13]=0;
    return off + bs;
}

static void test_calllog(void)
{
    nl_u8 o[20]; nl_u8 b[128]; long off; nl_pbk_entry e; long n;
    current = "req call register (dialled)";
    n = nl_pbk_req_read(o, sizeof o, NL_PBK_MEM_DIALLED, 0x0001);
    CHECK(n==14 && o[9]==0x01);
    CHECK(nl_pbk_req_read(o,sizeof o,NL_PBK_MEM_MISSED,1)==14);
    CHECK(nl_pbk_req_read(o,sizeof o,NL_PBK_MEM_RECEIVED,1)==14);
    CHECK(strcmp(nl_pbk_mem_name(NL_PBK_MEM_DIALLED),"dialled")==0);

    current = "call-log entry with time";
    memset(b,0,sizeof b); b[6]=0x01; off=18;
    off = text_block(b, off, PBK_NAME, "Pat Sample");
    off = num_block(b, off, 0x00, "07700900123");
    off = dt_block(b, off, 2026, 11, 17, 9, 15, 0);
    CHECK(nl_pbk_parse_entry(b, off, 1, &e) == NL_PBK_OK);
    CHECK(strcmp(e.name,"Pat Sample")==0 && e.nnumbers==1);
    CHECK(e.has_when && e.when.date==nl_date_make(2026,11,17));
    CHECK(e.when.secs == 9L*3600+15*60);

    current = "plain contact has no time";
    memset(b,0,sizeof b); b[6]=0x01; off=18;
    off = text_block(b, off, PBK_NAME, "No Time");
    CHECK(nl_pbk_parse_entry(b, off, 1, &e)==NL_PBK_OK && !e.has_when);
}

int main(void)
{
    test_requests();
    test_calllog();
    test_status();
    test_entry();
    test_sim_and_general();
    test_lost_and_cap();
    printf("pbk: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

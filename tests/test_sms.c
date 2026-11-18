/* Host tests for the messages phone-service (src/core/nlsms).
 * Run by tests/run-host.sh. All messages here are made up. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlsms.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static long pack7(const nl_u8 *s, int n, nl_u8 *out)
{
    int total = n * 7, nbytes = (total + 7) / 8, i;
    for (i = 0; i < nbytes; i++) out[i] = 0;
    for (i = 0; i < n; i++) {
        int bit = i * 7, byte = bit >> 3, shift = bit & 7;
        out[byte] |= (nl_u8)((s[i] << shift) & 0xFF);
        if (shift > 1) out[byte + 1] |= (nl_u8)(s[i] >> (8 - shift));
    }
    return nbytes;
}

/* Pack 7-bit septets starting `fill` bits in (for data after a UDH). */
static long pack7off(const nl_u8 *s, int n, nl_u8 *out, int fill, int cap)
{
    int totalbits = n * 7 + fill, nbytes = (totalbits + 7) / 8, i;
    for (i = 0; i < nbytes && i < cap; i++) out[i] = 0;
    for (i = 0; i < n; i++) {
        int bit = fill + i * 7, byte = bit >> 3, shift = bit & 7;
        out[byte] |= (nl_u8)((s[i] << shift) & 0xFF);
        if (shift > 1) out[byte + 1] |= (nl_u8)(s[i] >> (8 - shift));
    }
    return nbytes;
}

/* A 7-bit DELIVER carrying a concatenation header (one part of a long
 * message): the header must be reported and skipped, leaving just the text
 * septet-aligned past the fill bits. */
static void test_concat(void)
{
    nl_u8 b[96]; nl_u8 *f; nl_sms m; int r, i;
    const char *txt = "World";    /* this part's text */
    nl_u8 sept[8];
    long tn;
    memset(b, 0, sizeof b);
    b[4] = 0x01; f = b + 9;
    f[0] = 0;                     /* empty SMSC */
    f[12] = 0x40;                 /* deliver + UDH */
    f[14] = 0x00;                 /* 7-bit */
    f[16] = 0;                    /* empty sender */
    f[28]=0x62; f[29]=0x11; f[30]=0x71; f[31]=0x41; f[32]=0x03; f[33]=0x54; f[34]=0x40;
    /* user data: UDH (6 octets) then text at a 1-bit fill */
    f[35] = 0x05;                 /* UDHL */
    f[36] = 0x00; f[37] = 0x03;   /* IEI concat-8, length 3 */
    f[38] = 0xAB;                 /* reference */
    f[39] = 0x03;                 /* total parts */
    f[40] = 0x02;                 /* this is part 2 */
    for (i = 0; i < 5; i++) sept[i] = (nl_u8)txt[i];
    tn = pack7off(sept, 5, f + 41, 1, (int)(sizeof b - 9 - 41));
    f[15] = (nl_u8)(7 + 5);       /* UDL septets: 7 for the header + 5 text */

    current = "concat decode";
    r = nl_sms_parse_message(b, 9 + 41 + tn, &m);
    CHECK(r == NL_SMS_OK && m.udh && m.coding == NL_SMS_7BIT);
    CHECK(m.concat && m.concat_ref == 0xAB && m.concat_total == 3 && m.concat_seq == 2);
    CHECK(strcmp(m.text, "World") == 0 && m.text_lost == 0);

    current = "no concat without udh";
    memset(b, 0, sizeof b); b[4]=0x01; f=b+9;
    f[0]=0; f[12]=0x00; f[14]=0x00; f[16]=0; f[15]=5;
    f[28]=0x62; f[29]=0x11; f[30]=0x71; f[31]=0x41; f[32]=0x03; f[33]=0x54; f[34]=0x40;
    { nl_u8 s2[8]; for(i=0;i<5;i++) s2[i]=(nl_u8)"Hello"[i]; tn=pack7(s2,5,f+35); }
    CHECK(nl_sms_parse_message(b, 9+35+tn, &m)==NL_SMS_OK && !m.udh && !m.concat
          && strcmp(m.text,"Hello")==0);
}

static void test_folders(void)
{
    nl_u8 b[64]; nl_sms_folder fs[8]; int nf, i;
    nl_u16 locs[8]; int nl2;
    i = 0;
    b[0]=0;b[1]=1;b[2]=0; b[3]=0x7B; b[4]=2;         /* two folders */
    { nl_u8 *p=b+5; const char *n1="Inbox"; const char *n2="Outbox"; int k;
      *p++=0x08; for(k=0;n1[k];k++){*p++=0;*p++=(nl_u8)n1[k];} *p++=0;*p++=0;
      *p++=0x10; for(k=0;n2[k];k++){*p++=0;*p++=(nl_u8)n2[k];} *p++=0;*p++=0;
      i = (int)(p - b); }
    current = "folder list";
    CHECK(nl_sms_parse_folders(b, i, fs, 8, &nf) == NL_SMS_OK);
    CHECK(nf==2 && fs[0].id==0x08 && strcmp(fs[0].name,"Inbox")==0
          && fs[1].id==0x10 && strcmp(fs[1].name,"Outbox")==0);

    current = "folder status";
    memset(b,0,sizeof b); b[3]=0x6C; b[4]=0; b[5]=3;  /* three entries */
    b[6]=0;b[7]=1; b[8]=0;b[9]=2; b[10]=0;b[11]=5;     /* locations 1,2,5 */
    CHECK(nl_sms_parse_folder_status(b, 12, locs, 8, &nl2) == NL_SMS_OK);
    CHECK(nl2==3 && locs[0]==1 && locs[1]==2 && locs[2]==5);

    current = "folder requests";
    { nl_u8 o[8]; long r;
      r = nl_sms_req_folders(o, sizeof o);
      CHECK(r==6 && o[3]==0x7A);
      r = nl_sms_req_folder_status(o, sizeof o, 0x10);
      CHECK(r==7 && o[2]==0x01 && o[3]==0x6B && o[4]==0x10 && o[5]==0x0F && o[6]==0x01); }

    current = "delete request";
    { nl_u8 o[8]; long r = nl_sms_req_delete(o, sizeof o, 0x08, 5);
      CHECK(r==8 && o[2]==0x01 && o[3]==0x0A && o[4]==0x08 && o[5]==0 && o[6]==5 && o[7]==0x01); }
}

static void test_smsc(void)
{
    nl_u8 b[64]; char num[32], nm[32]; const char *name = "Test SC"; int i;
    memset(b,0,sizeof b);
    b[3]=0x34;                        /* SMSC reply OK */
    /* centre number at offset 21: len 4 (toa + 3 octets), intl +441234 */
    b[21]=4; b[22]=0x91; b[23]=0x44; b[24]=0x21; b[25]=0x43;
    for(i=0;name[i];i++) b[33+i]=(nl_u8)name[i];
    current = "smsc parse";
    CHECK(nl_sms_parse_smsc(b, 44, num, sizeof num, nm, sizeof nm)==NL_SMS_OK);
    CHECK(strcmp(num,"+441234")==0 && strcmp(nm,"Test SC")==0);
    current = "smsc req";
    { nl_u8 o[8]; long r = nl_sms_req_smsc(o,sizeof o,1);
      CHECK(r==6 && o[3]==0x33 && o[4]==0x64 && o[5]==1); }
    current = "smsc fail sub";
    b[3]=0x35; CHECK(nl_sms_parse_smsc(b,44,num,sizeof num,nm,sizeof nm)==NL_SMS_ERR_TYPE);
}

static void test_send(void)
{
    nl_u8 o[128]; long n; const nl_u8 *f;
    current = "send submit frame";
    n = nl_sms_req_send(o, sizeof o, "+447785014317", "+447700900123", "Hi");
    /* wrapper 00 01 00 01 02 00, then the SUBMIT frame */
    CHECK(o[0]==0 && o[1]==1 && o[2]==0 && o[3]==0x01 && o[4]==0x02 && o[5]==0x00);
    f = o + 6;
    CHECK(f[0]==7 && f[1]==0x91);                 /* SMSC: 7 octets, international */
    CHECK(f[2]==0x44 && f[7]==0x71);              /* SMSC BCD ends 17 -> 0x71 */
    CHECK(f[12]==0x11);                           /* SUBMIT + relative validity */
    CHECK(f[15]==0x00 && f[16]==2);               /* DCS 7-bit; TP-UDL = 2 septets */
    CHECK(f[17]==12 && f[18]==0x91);              /* recipient: 12 digits, international */
    CHECK(f[19]==0x44 && f[24]==0x32);            /* recipient BCD */
    CHECK(f[29]==0xAA);                           /* validity */
    CHECK(f[36]==0xC8 && f[37]==0x34);            /* "Hi" packed 7-bit */
    CHECK(n == 6 + 36 + 2);

    current = "send reply ok/fail";
    { nl_u8 r[8]; int ref = -1, ec = -1;
      memset(r,0,sizeof r); r[3]=0x02; r[5]=0x07;
      CHECK(nl_sms_parse_send(r, 6, &ref, &ec)==NL_SMS_OK && ref==7);
      r[3]=0x03; r[6]=0x1C;
      CHECK(nl_sms_parse_send(r, 7, &ref, &ec)!=NL_SMS_OK && ec==0x1C); }
}

static void test_requests(void)
{
    nl_u8 o[8]; long n;
    current = "req status";
    n = nl_sms_req_status(o, sizeof o);
    CHECK(n==5 && o[3]==0x36 && o[4]==0x64);
    current = "req read";
    n = nl_sms_req_read(o, sizeof o, NL_SMS_FOLDER_INBOX, 0x0005);
    CHECK(n==7 && o[3]==0x07 && o[4]==0x08 && o[5]==0x00 && o[6]==0x05);
}

static void test_status(void)
{
    nl_u8 b[20]; nl_sms_status st; int r;
    memset(b,0,sizeof b);
    b[3]=0x37;
    b[8]=0; b[9]=50;      /* sim size 50 */
    b[10]=0; b[11]=12;    /* phone used 12 */
    b[12]=0; b[13]=3;     /* phone unread 3 */
    b[14]=0; b[15]=8;     /* sim used 8 */
    b[16]=0; b[17]=1;     /* sim unread 1 */
    current = "status";
    r = nl_sms_parse_status(b, 18, &st);
    CHECK(r==NL_SMS_OK && st.sim_size==50 && st.phone_used==12 && st.phone_unread==3
          && st.sim_used==8 && st.sim_unread==1);
    current = "status short";
    CHECK(nl_sms_parse_status(b, 10, &st)==NL_SMS_ERR_SHORT);
}

static void test_deliver(void)
{
    nl_u8 b[80]; nl_u8 *f; nl_sms m; int r; long tn;
    const char *txt = "Hello";
    nl_u8 sept[8]; int i;
    memset(b,0,sizeof b);
    b[4] = 0x01;                     /* state */
    f = b + 9;
    /* SMSC +447785900000 (non-semioctet: len = 1 toa + 6 octets = 7) */
    f[0]=7; f[1]=0x91; f[2]=0x44; f[3]=0x77; f[4]=0x58; f[5]=0x09; f[6]=0x00; f[7]=0x00;
    f[12]=0x00;                      /* firstbyte: deliver, no UDH */
    f[14]=0x00;                      /* DCS: 7-bit */
    f[15]=5;                         /* UDL */
    /* sender +447700900123 (semi-octet: 12 digits) */
    f[16]=12; f[17]=0x91; f[18]=0x44; f[19]=0x77; f[20]=0x00; f[21]=0x09; f[22]=0x10; f[23]=0x32;
    /* time 2026-11-17 14:30:45 +1h (tz +4 quarters) */
    f[28]=0x62; f[29]=0x11; f[30]=0x71; f[31]=0x41; f[32]=0x03; f[33]=0x54; f[34]=0x40;
    for (i=0;i<5;i++) sept[i]=(nl_u8)txt[i];
    tn = pack7(sept, 5, f+35);
    r = nl_sms_parse_message(b, 9 + 35 + tn, &m);

    current = "deliver decode";
    CHECK(r==NL_SMS_OK);
    CHECK(m.pdu==NL_SMS_DELIVER && m.coding==NL_SMS_7BIT && m.state==0x01 && !m.udh);
    CHECK(strcmp(m.number,"+447700900123")==0);
    CHECK(strcmp(m.smsc,"+447785900000")==0);
    CHECK(m.has_time && m.time.date==nl_date_make(2026,11,17));
    CHECK(m.time.secs==14L*3600+30*60+45 && m.tz_quarters==4);
    CHECK(strcmp(m.text,"Hello")==0 && m.text_lost==0);
}

static void test_submit_ucs2(void)
{
    nl_u8 b[64]; nl_u8 *f; nl_sms m; int r;
    memset(b,0,sizeof b);
    b[4]=0x05; f=b+9;
    f[0]=0;                          /* empty SMSC */
    f[12]=0x01;                      /* submit */
    f[15]=0x08;                      /* DCS: UCS-2 ((0x08>>2)&3==2) */
    f[16]=4;                         /* UDL = 4 bytes */
    f[17]=4; f[18]=0x81; f[19]=0x70; f[20]=0x07;   /* recipient "0770", national */
    f[36]=0x00; f[37]=0x48; f[38]=0x00; f[39]=0x69;  /* "Hi" UCS-2 */
    r = nl_sms_parse_message(b, 9 + 40, &m);
    current = "submit ucs2";
    CHECK(r==NL_SMS_OK && m.pdu==NL_SMS_SUBMIT && m.coding==NL_SMS_UCS2);
    CHECK(strcmp(m.number,"0770")==0 && m.smsc[0]==0 && !m.has_time);
    CHECK(strcmp(m.text,"Hi")==0);
}

static void test_8bit_and_errors(void)
{
    nl_u8 b[64]; nl_u8 *f; nl_sms m; int r;
    memset(b,0,sizeof b);
    b[4]=0x01; f=b+9;
    f[0]=0; f[12]=0x00; f[14]=0x04;  /* deliver, DCS 8-bit ((0x04>>2)&3==1) */
    f[15]=3;                         /* UDL 3 bytes */
    f[16]=0;                         /* empty sender */
    f[28]=0x62; f[29]=0x11; f[30]=0x71; f[31]=0x41; f[32]=0x03; f[33]=0x54; f[34]=0x40;
    f[35]='O'; f[36]='K'; f[37]='!';
    r = nl_sms_parse_message(b, 9+38, &m);
    current = "8-bit text";
    CHECK(r==NL_SMS_OK && m.coding==NL_SMS_8BIT && strcmp(m.text,"OK!")==0);

    current = "short frame";
    CHECK(nl_sms_parse_message(b, 10, &m)==NL_SMS_ERR_SHORT);
}

int main(void)
{
    test_requests();
    test_status();
    test_deliver();
    test_submit_ucs2();
    test_8bit_and_errors();
    test_concat();
    test_folders();
    test_smsc();
    test_send();
    printf("sms: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

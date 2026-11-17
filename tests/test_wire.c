/* End-to-end: each phone service driven through the FBUS link against
 * the pretend phone. Proves the request builders, the link framing and
 * reassembly, and the reply decoders work together. Run by
 * tests/run-host.sh. All data is made up. */
#include <stdio.h>
#include <string.h>
#include "fakephone.h"
#include "../src/core/nlinfo.h"
#include "../src/core/nlcal.h"
#include "../src/core/nlpbk.h"
#include "../src/core/nlsms.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static nl_u8 g_msgbuf[4096];

/* Fresh pretend phone + started link, with one canned reply. */
static void setup(fakephone *fp, nl_link *l,
                  nl_u8 req_type, nl_u8 reply_type,
                  const nl_u8 *reply, long reply_len, long chunk)
{
    fp_reply r;
    fp_init(fp);
    nl_link_init(l, &fp->port, g_msgbuf, sizeof g_msgbuf, 0, 0);
    CHECK(nl_link_start_dlr3(l) == NL_OK);
    memset(&r, 0, sizeof r);
    r.req_type = req_type; r.reply_type = reply_type;
    r.data = reply; r.len = reply_len; r.chunk = chunk;
    fp_add_reply(fp, &r);
}

static long pack7(const nl_u8 *s, int n, nl_u8 *out)
{
    int total = n * 7, nb = (total + 7) / 8, i;
    for (i = 0; i < nb; i++) out[i] = 0;
    for (i = 0; i < n; i++) {
        int bit = i*7, by = bit>>3, sh = bit&7;
        out[by] |= (nl_u8)((s[i]<<sh)&0xFF);
        if (sh>1) out[by+1] |= (nl_u8)(s[i]>>(8-sh));
    }
    return nb;
}

static void test_identify(void)
{
    fakephone fp; nl_link l; nl_u8 req[8], reply[256]; long rn, len;
    nl_info_ident id;
    nl_u8 body[64]; long n = 4; const char *t = "V 04.80\r\n23-02-00\r\nNSE-5\r\n";
    memset(body,0,sizeof body); body[3]=0xD2;
    memcpy(body+n, t, strlen(t)); n += (long)strlen(t);
    current = "wire: identify";
    setup(&fp, &l, NL_INFO_TYPE_IDENTIFY, 0xD2, body, n, 200);
    rn = nl_info_req_identify(req, sizeof req);
    CHECK(nl_link_request(&l, NL_INFO_TYPE_IDENTIFY, req, rn, 0xD2,
                          reply, sizeof reply, &len, 2000) == NL_OK);
    CHECK(nl_info_parse_identify(reply, len, &id) == NL_INFO_OK);
    CHECK(strcmp(id.model,"NSE-5")==0 && strcmp(id.version,"04.80")==0);
}

static void test_clock(void)
{
    fakephone fp; nl_link l; nl_u8 req[8], reply[64]; long rn, len; nl_stamp st;
    nl_u8 body[16];
    memset(body,0,sizeof body);
    body[3]=0x63; body[4]=0x01;
    body[8]=(2026>>8); body[9]=(2026&0xFF); body[10]=11; body[11]=17; body[12]=14; body[13]=30; body[14]=45;
    current = "wire: clock";
    setup(&fp, &l, NL_INFO_TYPE_CLOCK, 0x19, body, 15, 200);
    rn = nl_info_req_clock(req, sizeof req);
    CHECK(nl_link_request(&l, NL_INFO_TYPE_CLOCK, req, rn, 0x19,
                          reply, sizeof reply, &len, 2000) == NL_OK);
    CHECK(nl_info_parse_clock(reply, len, &st) == NL_INFO_OK);
    CHECK(st.date == nl_date_make(2026,11,17) && st.secs == 14L*3600+30*60+45);
}

static void test_battery(void)
{
    fakephone fp; nl_link l; nl_u8 req[8], reply[32]; long rn, len; int p;
    nl_u8 body[8];
    memset(body,0,sizeof body); body[3]=0x03; body[5]=77;
    current = "wire: battery";
    setup(&fp, &l, NL_INFO_TYPE_BATTERY, 0x17, body, 6, 200);
    rn = nl_info_req_battery(req, sizeof req);
    CHECK(nl_link_request(&l, NL_INFO_TYPE_BATTERY, req, rn, 0x17,
                          reply, sizeof reply, &len, 2000) == NL_OK);
    CHECK(nl_info_parse_battery(reply, len, &p) == NL_INFO_OK && p == 77);
}

static void test_calendar(void)
{
    fakephone fp; nl_link l; nl_u8 req[8], reply[64]; long rn, len;
    nl_u16 total, locs[8]; int nlocs;
    nl_u8 body[16];
    memset(body,0,sizeof body);
    body[3]=0x3B; body[5]=2; body[6]=0x01; body[7]=0x07;
    body[8]=0; body[9]=1; body[10]=0; body[11]=4;    /* locations 1, 4 */
    current = "wire: calendar notes-info";
    setup(&fp, &l, NL_CAL_MSG_TYPE, NL_CAL_MSG_TYPE, body, 12, 200);
    rn = nl_cal_req_notes_info(req, sizeof req, 0);
    CHECK(nl_link_request(&l, NL_CAL_MSG_TYPE, req, rn, NL_CAL_MSG_TYPE,
                          reply, sizeof reply, &len, 2000) == NL_OK);
    CHECK(nl_cal_parse_notes_info(reply, len, &total, locs, 8, &nlocs) == NL_CAL_OK);
    CHECK(total==2 && nlocs==2 && locs[0]==1 && locs[1]==4);
}

static void test_contact_multiframe(void)
{
    fakephone fp; nl_link l; nl_u8 req[16], reply[256]; long rn, len;
    nl_pbk_entry e;
    nl_u8 body[128]; long off; int i;
    const char *name = "Pat Sample";
    memset(body,0,sizeof body); body[6]=0x01; off=18;
    /* NAME block */
    { int tb=2*(int)strlen(name); long bs=6+tb;
      body[off]=0x07; body[off+2]=(nl_u8)(bs>>8); body[off+3]=(nl_u8)bs; body[off+5]=(nl_u8)tb;
      for(i=0;name[i];i++){ body[off+6+2*i]=0; body[off+7+2*i]=(nl_u8)name[i]; } off+=bs; }
    /* NUMBER block (mobile) */
    { const char *num="07700900123"; int tb=2*(int)strlen(num); long bs=10+tb;
      body[off]=0x0B; body[off+2]=(nl_u8)(bs>>8); body[off+3]=(nl_u8)bs; body[off+5]=0x03; body[off+9]=(nl_u8)tb;
      for(i=0;num[i];i++){ body[off+10+2*i]=0; body[off+11+2*i]=(nl_u8)num[i]; } off+=bs; }
    current = "wire: contact (multi-frame)";
    setup(&fp, &l, NL_PBK_MSG_TYPE, NL_PBK_MSG_TYPE, body, off, 40);  /* small chunk -> several frames */
    rn = nl_pbk_req_read(req, sizeof req, NL_PBK_MEM_ME, 7);
    CHECK(nl_link_request(&l, NL_PBK_MSG_TYPE, req, rn, NL_PBK_MSG_TYPE,
                          reply, sizeof reply, &len, 2000) == NL_OK);
    CHECK(fp.frames_seen >= 2 || len > 40);   /* reassembled from more than one frame */
    CHECK(nl_pbk_parse_entry(reply, len, 7, &e) == NL_PBK_OK);
    CHECK(strcmp(e.name,"Pat Sample")==0 && e.nnumbers==1
          && strcmp(e.numbers[0].text,"07700900123")==0);
}

static void test_sms(void)
{
    fakephone fp; nl_link l; nl_u8 req[8], reply[128]; long rn, len;
    nl_sms m; nl_u8 body[80], *f; nl_u8 sept[8]; int i; const char *txt="Hello"; long tn;
    memset(body,0,sizeof body); body[4]=0x01; f=body+9;
    f[0]=0; f[12]=0x00; f[14]=0x00; f[15]=5;
    f[16]=12; f[17]=0x91; f[18]=0x44; f[19]=0x77; f[20]=0x00; f[21]=0x09; f[22]=0x10; f[23]=0x32;
    f[28]=0x62; f[29]=0x11; f[30]=0x71; f[31]=0x41; f[32]=0x03; f[33]=0x54; f[34]=0x40;
    for(i=0;i<5;i++) sept[i]=(nl_u8)txt[i];
    tn = pack7(sept,5,f+35);
    current = "wire: sms deliver";
    setup(&fp, &l, NL_SMS_MSG_TYPE, NL_SMS_MSG_TYPE, body, 9+35+tn, 200);
    rn = nl_sms_req_read(req, sizeof req, NL_SMS_FOLDER_INBOX, 5);
    CHECK(nl_link_request(&l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE,
                          reply, sizeof reply, &len, 2000) == NL_OK);
    CHECK(nl_sms_parse_message(reply, len, &m) == NL_SMS_OK);
    CHECK(m.pdu==NL_SMS_DELIVER && strcmp(m.number,"+447700900123")==0
          && strcmp(m.text,"Hello")==0);
}

int main(void)
{
    test_identify();
    test_clock();
    test_battery();
    test_calendar();
    test_contact_multiframe();
    test_sms();
    printf("wire: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

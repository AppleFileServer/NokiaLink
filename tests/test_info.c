/* Host tests for phone status and clock (src/core/nlinfo).
 * Run by tests/run-host.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlinfo.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static void test_requests(void)
{
    nl_u8 o[8]; long n;
    current = "req identify";
    n = nl_info_req_identify(o, sizeof o);
    CHECK(n==5 && o[0]==0&&o[1]==1&&o[2]==0&&o[3]==0x03&&o[4]==0x00);
    current = "req imei";
    n = nl_info_req_imei(o, sizeof o);
    CHECK(n==4 && o[2]==0x66 && o[3]==0x00);
    current = "req battery/signal/network/clock subs";
    nl_info_req_battery(o,sizeof o); CHECK(o[3]==0x02);
    nl_info_req_signal(o,sizeof o);  CHECK(o[3]==0x81);
    nl_info_req_network(o,sizeof o); CHECK(o[3]==0x70);
    nl_info_req_clock(o,sizeof o);   CHECK(o[3]==0x62);
}

static void test_identify(void)
{
    nl_u8 b[64]; nl_info_ident id; int r; long n=0; const char *t;
    memset(b,0,sizeof b);
    b[0]=0x00; b[1]=0x01; b[2]=0x00; b[3]=0xD2;   /* non-printable header */
    n=4;
    t = "V 04.80\r\n23-02-00\r\nNSE-5\r\n(c) NMP.\r\n";
    memcpy(b+n, t, strlen(t)); n += (long)strlen(t);
    r = nl_info_parse_identify(b, n, &id);
    current = "identify";
    CHECK(r==NL_INFO_OK);
    CHECK(strcmp(id.model,"NSE-5")==0);
    CHECK(strcmp(id.version,"04.80")==0);
    CHECK(strcmp(id.date,"23-02-00")==0);
}

static void test_imei(void)
{
    nl_u8 b[32]; char imei[20]; int r;
    memset(b,0,sizeof b);
    memcpy(b+4, "490154203237518", 15);
    b[19]=0x00;
    current = "imei";
    r = nl_info_parse_imei(b, 20, imei, sizeof imei);
    CHECK(r==NL_INFO_OK && strcmp(imei,"490154203237518")==0);
}

static void test_battery_signal(void)
{
    nl_u8 b[8]; int p, r;
    current = "battery";
    memset(b,0,sizeof b); b[5]=85;
    r = nl_info_parse_battery(b, 6, &p);
    CHECK(r==NL_INFO_OK && p==85);
    current = "signal";
    memset(b,0,sizeof b); b[3]=0x82; b[4]=60;
    r = nl_info_parse_signal(b, 5, &p);
    CHECK(r==NL_INFO_OK && p==60);
    current = "signal wrong sub";
    b[3]=0x00; CHECK(nl_info_parse_signal(b,5,&p)==NL_INFO_ERR_SUB);
}

static void test_network(void)
{
    nl_u8 b[64]; nl_info_network net; int r; const char *nm="Itineris"; int i;
    memset(b,0,sizeof b);
    b[3]=0x71; b[8]=0x01;                 /* registered */
    b[10]=0x12; b[11]=0x34;               /* CID 1234 */
    b[12]=0xAB; b[13]=0xCD;               /* LAC ABCD */
    b[14]=0x32; b[15]=0xF4; b[16]=0x51;   /* MCC 234 MNC 15 */
    b[17]=8;                              /* name length (chars) */
    for (i=0;i<8;i++){ b[18+2*i]=0; b[19+2*i]=(nl_u8)nm[i]; }
    r = nl_info_parse_network(b, 18+16, &net);
    current = "network registered";
    CHECK(r==NL_INFO_OK && net.registered);
    CHECK(strcmp(net.code,"234 15")==0);
    CHECK(strcmp(net.cid,"1234")==0 && strcmp(net.lac,"ABCD")==0);
    CHECK(strcmp(net.name,"Itineris")==0 && net.name_lost==0);

    current = "network not registered";
    b[8]=0x05;
    r = nl_info_parse_network(b, 18+16, &net);
    CHECK(r==NL_INFO_OK && !net.registered);
}

static void test_clock(void)
{
    nl_u8 b[16]; nl_stamp st; int r;
    memset(b,0,sizeof b);
    b[3]=0x63; b[4]=0x01;
    b[8]=(2026>>8); b[9]=(2026&0xFF); b[10]=11; b[11]=17; b[12]=14; b[13]=30; b[14]=45;
    current = "clock";
    r = nl_info_parse_clock(b, 15, &st);
    CHECK(r==NL_INFO_OK && st.date==nl_date_make(2026,11,17));
    CHECK(st.secs==14L*3600+30*60+45);
    current = "clock not set";
    b[4]=0x02;
    CHECK(nl_info_parse_clock(b,15,&st)==NL_INFO_ERR_NODATA);
    current = "clock short";
    CHECK(nl_info_parse_clock(b,10,&st)==NL_INFO_ERR_SHORT);
}

static void test_set_clock(void)
{
    nl_u8 o[16]; long n; nl_u8 ok[8], bad[8];
    current = "set-clock request";
    n = nl_info_req_set_clock(o, sizeof o, nl_date_make(2026, 11, 18), 22L*3600 + 42*60 + 5);
    CHECK(n == 14 && o[3]==0x60 && o[4]==0x01 && o[5]==0x01 && o[6]==0x07);
    CHECK(o[7]==(2026>>8) && o[8]==(2026&0xFF) && o[9]==11 && o[10]==18 && o[11]==22 && o[12]==42 && o[13]==0x00);
    current = "set-clock reply ok";
    memset(ok,0,sizeof ok); ok[3]=0x61; ok[4]=0x01;
    CHECK(nl_info_parse_set_clock(ok, 5) == NL_INFO_OK);
    current = "set-clock reply fail";
    memset(bad,0,sizeof bad); bad[3]=0x61; bad[4]=0x00;
    CHECK(nl_info_parse_set_clock(bad, 5) != NL_INFO_OK);
}

int main(void)
{
    test_requests();
    test_identify();
    test_imei();
    test_battery_signal();
    test_network();
    test_clock();
    test_set_clock();
    printf("info: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

/* NokiaLink smoke test (see smoke.h). Runs on the 68000 under Mini vMac. */
#include <Files.h>
#include <Memory.h>
#include <OSUtils.h>
#include <string.h>

#include "smoke.h"
#include "nldate.h"
#include "nlnumber.h"
#include "nltext.h"
#include "nlcrc.h"
#include "nlvcard.h"
#include "nlcontact.h"
#include "nlcal.h"
#include "nlpbk.h"
#include "nlgsm.h"
#include "nlsms.h"
#include "nlinfo.h"
#include "nlsync.h"
#include "nlbase.h"
#include "nlctsync.h"

static int g_fail, g_checks;

/* Open a file's data fork via a cleared parameter block. Retro68's HOpen
 * glue leaves the block partly uninitialised and can hang or crash, so
 * use this instead (copied from the suite's MacMoney/MacTrack). */
static OSErr OpenDataFork(short vRefNum, long dirID, ConstStringPtr name,
                          SignedByte permission, short *refNum)
{
    HParamBlockRec pb;
    OSErr err;
    memset(&pb, 0, sizeof pb);
    pb.ioParam.ioNamePtr = (StringPtr)name;
    pb.ioParam.ioVRefNum = vRefNum;
    pb.ioParam.ioPermssn = permission;
    pb.fileParam.ioDirID = dirID;
    err = PBHOpenSync(&pb);
    *refNum = pb.ioParam.ioRefNum;
    return err;
}

/* Append a line to the file "out" on the boot disk; LaunchAPPL reads it
 * back and prints it, as the other suite programs' smoke tests do. */
static void Log(const char *s)
{
    short ref;
    long n = (long)strlen(s), one = 1;
    char nl = '\n';
    HCreate(0, 0, (ConstStringPtr)"\pout", 'ttxt', 'TEXT');
    if (OpenDataFork(0, 0, (ConstStringPtr)"\pout", fsRdWrPerm, &ref) != noErr) return;
    SetFPos(ref, fsFromLEOF, 0);
    FSWrite(ref, &n, s);
    FSWrite(ref, &one, &nl);
    FSClose(ref);
}

static void ck(int cond, const char *name)
{
    g_checks++;
    if (!cond) { g_fail++; Log(name); }
}
#define CK(cond, name) ck((cond) ? 1 : 0, "FAIL: " name)

/* append UCS-2 big-endian of an ASCII string; returns bytes */
static long be(nl_u8 *p, const char *s)
{ long n = 0; while (*s) { p[n++] = 0; p[n++] = (nl_u8)*s++; } return n; }

static long pack7(const nl_u8 *s, int n, nl_u8 *out)
{
    int total = n*7, nb = (total+7)/8, i;
    for (i = 0; i < nb; i++) out[i] = 0;
    for (i = 0; i < n; i++) {
        int bit = i*7, by = bit>>3, sh = bit&7;
        out[by] |= (nl_u8)((s[i]<<sh)&0xFF);
        if (sh>1) out[by+1] |= (nl_u8)(s[i]>>(8-sh));
    }
    return nb;
}

static void run_checks(void)
{
    /* dates */
    CK(nl_date_make(2026,11,17)==20261117L, "date_make");
    CK(nl_date_add_months(20260831L,-18)==20250228L, "add_months clamp");
    CK(nl_date_to_days(19050101L)==366L, "to_days 1904 leap");
    CK(nl_date_from_days(nl_date_to_days(20420101L))==20420101L, "days roundtrip 2042");

    /* numbers */
    { char d[NL_NUMBER_MAX];
      CK(nl_number_same("07700 900123","+447700900123"), "number_same");
      CK(nl_number_digits("+44 (0)7700",d)==7, "number_digits"); }

    /* text: UCS-2 big-endian <-> Mac Roman (byte-order sensitive) */
    { nl_u8 u[8]; char out[16]; long lost;
      u[0]=0x00; u[1]=0xE9; u[2]=0x00; u[3]=0xA3;   /* e-acute, pound */
      CK(nl_ucs2_to_macroman(u,2,out,sizeof out,&lost)==2 && (nl_u8)out[0]==0x8E
         && (nl_u8)out[1]==0xA3 && lost==0, "ucs2->macroman"); }

    /* crc32 */
    CK(nl_crc32("123456789",9)==0xCBF43926UL, "crc32 check value");

    /* vcard */
    { static nl_vcard v; char val[64];
      CK(nl_vcard_load(&v,"BEGIN:VCARD\rVERSION:2.1\rFN:Pat\rTEL;CELL:12345\rEND:VCARD\r",57)==NL_VC_OK, "vcard load");
      CK(nl_vcard_get(&v,"FN",0,val,sizeof val)==3 && strcmp(val,"Pat")==0, "vcard get");
      CK(nl_vcard_get(&v,"TEL",0,val,sizeof val)==5, "vcard tel"); }

    /* contact hash stable + ignores format */
    { static nl_vcard a, b; nl_u32 ha, hb;
      nl_vcard_load(&a,"BEGIN:VCARD\rVERSION:2.1\rN:S;P\rTEL;CELL:+447700900123\rUID:X\rEND:VCARD\r",70);
      nl_vcard_load(&b,"BEGIN:VCARD\rVERSION:2.1\rUID:X\rTEL;CELL:07700 900123\rN:S;P\rEND:VCARD\r",69);
      ha=nl_contact_hash(&a); hb=nl_contact_hash(&b);
      CK(ha==hb, "contact hash format-independent"); }

    /* calendar note decode (u16 reads) */
    { nl_u8 b[48]; long n; nl_cal_note note;
      memset(b,0,sizeof b);
      b[0]=0;b[1]=1;b[2]=0;b[3]=0x1A;b[4]=0;b[5]=5;b[6]=0x01;b[7]=0;
      b[8]=(2026>>8);b[9]=(2026&0xFF);b[10]=11;b[11]=20;b[12]=14;b[13]=30;
      b[14]=0;b[15]=0x0F;b[16]=0;b[17]=0xA8;b[18]=3;b[19]=0;
      n=20+be(b+20,"Gym");
      CK(nl_cal_parse_note(b,n,&note)==NL_CAL_OK && note.type==NL_CAL_MEETING
         && note.date==nl_date_make(2026,11,20) && note.has_alarm && note.alarm_before==15
         && note.recur==NL_REC_WEEKLY && strcmp(note.text,"Gym")==0, "calendar note"); }

    /* phonebook entry */
    { nl_u8 b[80]; long off; nl_pbk_entry e; int tb; long bs; int i;
      memset(b,0,sizeof b); b[6]=0x01; off=18;
      tb=2*3; bs=6+tb; b[off]=0x07; b[off+2]=(nl_u8)(bs>>8); b[off+3]=(nl_u8)bs; b[off+5]=(nl_u8)tb;
      for(i=0;i<3;i++){b[off+6+2*i]=0;b[off+7+2*i]=(nl_u8)"Pat"[i];} off+=bs;
      tb=2*5; bs=10+tb; b[off]=0x0B; b[off+2]=(nl_u8)(bs>>8); b[off+3]=(nl_u8)bs; b[off+5]=0x03; b[off+9]=(nl_u8)tb;
      for(i=0;i<5;i++){b[off+10+2*i]=0;b[off+11+2*i]=(nl_u8)"12345"[i];} off+=bs;
      CK(nl_pbk_parse_entry(b,off,7,&e)==NL_PBK_OK && strcmp(e.name,"Pat")==0
         && e.nnumbers==1 && e.numbers[0].type==NL_NUM_MOBILE
         && strcmp(e.numbers[0].text,"12345")==0, "phonebook entry"); }

    /* gsm: 7-bit unpack, number, timestamp */
    { nl_u8 sp[8], pk[8]; char out[16]; long lost; int i;
      for(i=0;i<5;i++) sp[i]=(nl_u8)"Hello"[i];
      pack7(sp,5,pk);
      CK(nl_gsm7_to_macroman(pk,8,0,5,out,sizeof out,&lost)==5 && strcmp(out,"Hello")==0, "gsm 7-bit");
      { nl_u8 p[8]; char num[24];
        p[0]=12;p[1]=0x91;p[2]=0x44;p[3]=0x77;p[4]=0x00;p[5]=0x09;p[6]=0x10;p[7]=0x32;
        CK(nl_gsm_number(p,8,1,num,sizeof num)==13 && strcmp(num,"+447700900123")==0, "gsm number"); }
      { nl_u8 p[7]; nl_stamp st; int tz;
        p[0]=0x62;p[1]=0x11;p[2]=0x71;p[3]=0x41;p[4]=0x03;p[5]=0x54;p[6]=0x40;
        CK(nl_gsm_timestamp(p,7,&st,&tz)==NL_GSM_OK && st.date==nl_date_make(2026,11,17)
           && tz==4, "gsm timestamp"); } }

    /* sms deliver */
    { nl_u8 b[80]; nl_u8 *f; nl_sms m; nl_u8 sp[8]; long tn; int i;
      memset(b,0,sizeof b); b[4]=1; f=b+9;
      f[0]=0; f[12]=0; f[14]=0; f[15]=5;
      f[16]=12;f[17]=0x91;f[18]=0x44;f[19]=0x77;f[20]=0;f[21]=0x09;f[22]=0x10;f[23]=0x32;
      f[28]=0x62;f[29]=0x11;f[30]=0x71;f[31]=0x41;f[32]=0x03;f[33]=0x54;f[34]=0x40;
      for(i=0;i<5;i++) sp[i]=(nl_u8)"Hello"[i];
      tn=pack7(sp,5,f+35);
      CK(nl_sms_parse_message(b,9+35+tn,&m)==NL_SMS_OK && m.pdu==NL_SMS_DELIVER
         && strcmp(m.number,"+447700900123")==0 && strcmp(m.text,"Hello")==0, "sms deliver"); }

    /* phone info: identify, clock, network */
    { nl_u8 b[64]; nl_info_ident id; long n; const char *t="V 04.80\r23-02-00\rNSE-5\r"; int i;
      memset(b,0,sizeof b); b[3]=0xD2; for(n=4,i=0;t[i];i++) b[n++]=(nl_u8)t[i];
      CK(nl_info_parse_identify(b,n,&id)==NL_INFO_OK && strcmp(id.model,"NSE-5")==0
         && strcmp(id.version,"04.80")==0 && strcmp(id.date,"23-02-00")==0, "info identify"); }
    { nl_u8 b[64]; nl_info_ident id; long n; const char *t="V 04.80\r23-02-00\rNSE-5\r"; int i;
      char fw[40]; memset(b,0,sizeof b); b[3]=0xD2; for(n=4,i=0;t[i];i++) b[n++]=(nl_u8)t[i];
      fw[0]=0;
      if (nl_info_parse_identify(b,n,&id)==NL_INFO_OK) {
        strcpy(fw,"Firmware "); strcat(fw,id.version); strcat(fw,", "); strcat(fw,id.date); }
      Log(fw); CK(strcmp(fw,"Firmware 04.80, 23-02-00")==0, "firmware string built"); }

    /* phone info: clock + network */
    { nl_u8 b[48]; nl_stamp st; nl_info_network net; int i;
      memset(b,0,sizeof b); b[3]=0x63; b[4]=1;
      b[8]=(2026>>8);b[9]=(2026&0xFF);b[10]=11;b[11]=17;b[12]=14;b[13]=30;b[14]=45;
      CK(nl_info_parse_clock(b,15,&st)==NL_INFO_OK && st.date==nl_date_make(2026,11,17), "info clock");
      memset(b,0,sizeof b); b[3]=0x71;b[8]=1;b[10]=0x12;b[11]=0x34;b[12]=0xAB;b[13]=0xCD;
      b[14]=0x32;b[15]=0xF4;b[16]=0x51;b[17]=8;
      for(i=0;i<8;i++){b[18+2*i]=0;b[19+2*i]=(nl_u8)"Itineris"[i];}
      CK(nl_info_parse_network(b,34,&net)==NL_INFO_OK && net.registered
         && strcmp(net.code,"234 15")==0 && strcmp(net.name,"Itineris")==0, "info network"); }

    /* sync three-way compare */
    { nl_sync_item base[2], ph[2], st[2]; nl_sync_action out[4]; int n;
      base[0].key="k1"; base[0].hash=1; base[1].key="k2"; base[1].hash=1;
      ph[0].key="k1"; ph[0].hash=1; ph[1].key="k2"; ph[1].hash=2;
      st[0].key="k1"; st[0].hash=1; st[1].key="k2"; st[1].hash=1;
      CK(nl_sync_compare(base,2,ph,2,st,2,out,4,&n)==NL_SYNC_OK && n==1
         && out[0].verb==NL_SYNC_UPDATE_STORE, "sync compare");
      CK(nl_sync_decide(1,1,1,2,0,0)==NL_SYNC_CONFLICT, "sync conflict"); }

    /* base write/parse round-trip */
    { static nl_base bb, b2; char buf[128]; long n; nl_u32 h;
      nl_base_clear(&bb); nl_base_add(&bb,"ABC",0xDEADBEEFUL);
      n=nl_base_write(&bb,buf,sizeof buf);
      CK(n>0 && nl_base_parse(&b2,buf,n)==NL_BASE_OK && nl_base_find(&b2,"ABC",&h)
         && h==0xDEADBEEFUL, "base roundtrip"); }

    /* contacts policy */
    CK(nl_ct_policy(NL_SYNC_DELETE_FROM_STORE)==NL_CT_MOVE_TO_ARCHIVE, "ct policy archive");
}

void Smoke_Run(void)
{
    char line[32], num[12];
    long v, i, k;

    /* clear any previous "out" */
    HDelete(0, 0, (ConstStringPtr)"\pout");

    g_fail = 0; g_checks = 0;
    Log("NokiaLink smoke test");
    run_checks();

    /* "CHECKS: n" */
    strcpy(line, "CHECKS: ");
    v = g_checks; k = 0; if (v==0) num[k++]='0';
    { char t[12]; int tn=0; while(v){t[tn++]=(char)('0'+v%10); v/=10;} while(tn) num[k++]=t[--tn]; }
    num[k]=0; strcat(line, num); Log(line);

    /* "FAILURES: n" */
    strcpy(line, "FAILURES: ");
    v = g_fail; k = 0; if (v==0) num[k++]='0';
    { char t[12]; int tn=0; while(v){t[tn++]=(char)('0'+v%10); v/=10;} while(tn) num[k++]=t[--tn]; }
    num[k]=0; strcat(line, num); Log(line);

    Log("DONE");
    (void)i;
}

/* Host tests for the calendar phone-service (src/core/nlcal).
 * Run by tests/run-host.sh. All notes here are made up. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlcal.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static const nl_u8 HDR[4] = {0x00,0x01,0x00,0x00};

/* append a UCS-2 big-endian string of ASCII chars; returns bytes written */
static long puttext(nl_u8 *p, const char *s)
{
    long n = 0;
    for (; *s; s++) { p[n++] = 0x00; p[n++] = (nl_u8)*s; }
    return n;
}

static void test_requests(void)
{
    nl_u8 out[16];
    long n;
    current = "req notes-info first";
    n = nl_cal_req_notes_info(out, sizeof out, 0);
    CHECK(n == 6 && out[0]==0 && out[1]==1 && out[2]==0 && out[3]==0x3A
          && out[4]==0xFF && out[5]==0xFE);
    current = "req notes-info after";
    n = nl_cal_req_notes_info(out, sizeof out, 0x0007);
    CHECK(n == 6 && out[4]==0x00 && out[5]==0x07);
    current = "req get-note";
    n = nl_cal_req_get_note(out, sizeof out, 0x0005);
    CHECK(n == 6 && out[3]==0x19 && out[4]==0x00 && out[5]==0x05);
    current = "req first-free";
    n = nl_cal_req_first_free(out, sizeof out);
    CHECK(n == 4 && out[3]==0x31);
    current = "req no space";
    CHECK(nl_cal_req_get_note(out, 3, 1) == NL_CAL_ERR_SPACE);
}

static void test_notes_info(void)
{
    nl_u8 b[32]; nl_u16 total, locs[8]; int nlocs, r;
    current = "notes-info three";
    memcpy(b, HDR, 4); b[3]=0x3B; b[4]=0x00; b[5]=0x03; b[6]=0x01; b[7]=0x07;
    b[8]=0x00; b[9]=0x01; b[10]=0x00; b[11]=0x05; b[12]=0x00; b[13]=0x09;
    r = nl_cal_parse_notes_info(b, 14, &total, locs, 8, &nlocs);
    CHECK(r==NL_CAL_OK && total==3 && nlocs==3);
    CHECK(locs[0]==1 && locs[1]==5 && locs[2]==9);

    current = "notes-info stops at zero";
    b[12]=0x00; b[13]=0x00;                       /* a 0 location ends the list */
    r = nl_cal_parse_notes_info(b, 14, &total, locs, 8, &nlocs);
    CHECK(r==NL_CAL_OK && nlocs==2);

    current = "notes-info empty";
    b[8]=0x00; b[9]=0x00;
    r = nl_cal_parse_notes_info(b, 14, &total, locs, 8, &nlocs);
    CHECK(r==NL_CAL_ERR_EMPTY && nlocs==0);

    current = "notes-info wrong sub";
    memcpy(b, HDR, 4); b[3]=0x99;
    CHECK(nl_cal_parse_notes_info(b, 14, &total, locs, 8, &nlocs)==NL_CAL_ERR_SUB);

    current = "notes-info short";
    CHECK(nl_cal_parse_notes_info(b, 6, &total, locs, 8, &nlocs)==NL_CAL_ERR_SHORT);
}

static void test_meeting(void)
{
    nl_u8 b[64]; long n; nl_cal_note note; int r;
    current = "meeting";
    memcpy(b, HDR, 4); b[3]=0x1A; b[4]=0x00; b[5]=0x05;
    b[6]=0x01; b[7]=0x00;                 /* type meeting */
    b[8]=(2026>>8); b[9]=(2026&0xFF); b[10]=11; b[11]=20;   /* date */
    b[12]=14; b[13]=30;                   /* 14:30 */
    b[14]=0x00; b[15]=0x0F;               /* alarm 15 min before */
    b[16]=0x00; b[17]=0xA8;               /* recurrence 168h = weekly */
    b[18]=3; b[19]=0x00;                  /* text length 3 */
    n = 20 + puttext(b+20, "Gym");
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK);
    CHECK(note.type==NL_CAL_MEETING && note.location==5);
    CHECK(note.date==nl_date_make(2026,11,20));
    CHECK(note.hour==14 && note.minute==30);
    CHECK(note.has_alarm && note.alarm_before==15);
    CHECK(note.recur==NL_REC_WEEKLY && note.recur_hours==168);
    CHECK(strcmp(note.text,"Gym")==0 && note.text_lost==0);

    current = "meeting no alarm";
    b[14]=0xFF; b[15]=0xFF;
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK && !note.has_alarm);

    current = "meeting no recur, on-time alarm";
    b[14]=0x00; b[15]=0x00; b[16]=0x00; b[17]=0x00;
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK && note.has_alarm && note.alarm_before==0 && note.recur==NL_REC_NONE);

    current = "meeting truncated text";
    CHECK(nl_cal_parse_note(b, 24, &note)==NL_CAL_ERR_SHORT);  /* says 3 chars, only 2 present */
}

static void test_call(void)
{
    nl_u8 b[80]; long n, off; nl_cal_note note; int r;
    current = "call";
    memcpy(b, HDR, 4); b[3]=0x1A; b[4]=0x00; b[5]=0x02;
    b[6]=0x02; b[7]=0x00;
    b[8]=(2026>>8); b[9]=(2026&0xFF); b[10]=12; b[11]=1;
    b[12]=9; b[13]=0;
    b[14]=0xFF; b[15]=0xFF;               /* no alarm */
    b[16]=0x00; b[17]=0x18;               /* 24h = daily */
    b[18]=4;                              /* text len "Call" */
    b[19]=6;                              /* number len "123456" */
    off = 20 + puttext(b+20, "Call");
    off += puttext(b+off, "123456");
    n = off;
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK && note.type==NL_CAL_CALL);
    CHECK(strcmp(note.text,"Call")==0);
    CHECK(strcmp(note.number,"123456")==0 && note.number_lost==0);
    CHECK(note.recur==NL_REC_DAILY);
}

static void test_reminder(void)
{
    nl_u8 b[48]; long n; nl_cal_note note; int r;
    current = "reminder";
    memcpy(b, HDR, 4); b[3]=0x1A; b[4]=0x00; b[5]=0x07;
    b[6]=0x08; b[7]=0x00;
    b[8]=(2027>>8); b[9]=(2027&0xFF); b[10]=1; b[11]=15;
    b[12]=0x00; b[13]=0x00;               /* recurrence none (at 12..13) */
    b[14]=5; b[15]=0x00;                  /* text length 5 */
    n = 16 + puttext(b+16, "Note!");
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK && note.type==NL_CAL_REMINDER);
    CHECK(note.date==nl_date_make(2027,1,15));
    CHECK(note.hour==0 && note.minute==0 && !note.has_alarm);
    CHECK(strcmp(note.text,"Note!")==0);
}

static void test_birthday(void)
{
    nl_u8 b[48]; long n; nl_cal_note note; int r;
    current = "birthday";
    memcpy(b, HDR, 4); b[3]=0x1A; b[4]=0x00; b[5]=0x03;
    b[6]=0x04; b[7]=0x00;
    b[8]=0x00; b[9]=0x00;                 /* year here is not used */
    b[10]=6; b[11]=24;                    /* 24 June */
    b[12]=0x00; b[13]=0x00;
    b[14]=0x00; b[15]=0x00; b[16]=0xFF; b[17]=0xFF;   /* no alarm (0x0000FFFF) */
    b[18]=(1990>>8); b[19]=(1990&0xFF);  /* year of birth */
    b[20]=0x00;                          /* alarm type */
    b[21]=3;                             /* text length */
    n = 22 + puttext(b+22, "Sam");
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK && note.type==NL_CAL_BIRTHDAY);
    CHECK(nl_date_month(note.date)==6 && nl_date_day(note.date)==24);
    CHECK(note.birth_year==1990 && !note.has_alarm);
    CHECK(note.recur==NL_REC_YEARLY);
    CHECK(strcmp(note.text,"Sam")==0);

    current = "birthday silent alarm, no year";
    b[16]=0x00; b[17]=0x3C;              /* alarm = 60 seconds before (not 0xFFFF) */
    b[18]=0xFF; b[19]=0xFF;              /* no year of birth */
    b[20]=0x01;                          /* silent */
    r = nl_cal_parse_note(b, n, &note);
    CHECK(r==NL_CAL_OK && note.has_alarm && note.alarm_silent && note.alarm_before==60);
    CHECK(note.birth_year==0);
}

static void test_errors(void)
{
    nl_u8 b[32]; nl_cal_note note;
    current = "note wrong sub";
    memcpy(b, HDR, 4); b[3]=0x99;
    CHECK(nl_cal_parse_note(b, 20, &note)==NL_CAL_ERR_SUB);
    current = "note unknown type";
    memcpy(b, HDR, 4); b[3]=0x1A; b[6]=0x10;
    CHECK(nl_cal_parse_note(b, 20, &note)==NL_CAL_ERR_TYPE);
    current = "note short header";
    CHECK(nl_cal_parse_note(b, 10, &note)==NL_CAL_ERR_SHORT);
}

static void test_lost_chars(void)
{
    nl_u8 b[64]; long n; nl_cal_note note;
    current = "note text with non-mac-roman char";
    memcpy(b, HDR, 4); b[3]=0x1A; b[4]=0; b[5]=1;
    b[6]=0x08; b[7]=0; b[8]=(2026>>8); b[9]=(2026&0xFF); b[10]=5; b[11]=1;
    b[12]=0; b[13]=0; b[14]=3; b[15]=0;
    /* text: 'A', U+0416 (Cyrillic), 'Z' */
    b[16]=0x00; b[17]='A'; b[18]=0x04; b[19]=0x16; b[20]=0x00; b[21]='Z';
    n = 22;
    CHECK(nl_cal_parse_note(b, n, &note)==NL_CAL_OK);
    CHECK(strcmp(note.text,"A?Z")==0 && note.text_lost==1);
}

int main(void)
{
    test_requests();
    test_notes_info();
    test_meeting();
    test_call();
    test_reminder();
    test_birthday();
    test_errors();
    test_lost_chars();
    printf("cal: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

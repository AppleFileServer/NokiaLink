/* Calendar notes over the cable (see nlcal.h). */
#include <string.h>

#include "nlcal.h"

#define HDR0 0x00
#define HDR1 0x01
#define HDR2 0x00

static long u16(const nl_u8 *p) { return (p[0] << 8) | p[1]; }

/* ---- Requests ------------------------------------------------------- */

static long req_hdr(nl_u8 *out, long outsize, nl_u8 sub, long extra)
{
    if (outsize < 4 + extra) return NL_CAL_ERR_SPACE;
    out[0] = HDR0; out[1] = HDR1; out[2] = HDR2; out[3] = sub;
    return 4;
}

long nl_cal_req_notes_info(nl_u8 *out, long outsize, nl_u16 after)
{
    long n = req_hdr(out, outsize, 0x3A, 2);
    if (n < 0) return n;
    if (after == 0) { out[4] = 0xFF; out[5] = 0xFE; }   /* "from the start" */
    else { out[4] = (nl_u8)(after >> 8); out[5] = (nl_u8)after; }
    return 6;
}

long nl_cal_req_get_note(nl_u8 *out, long outsize, nl_u16 location)
{
    long n = req_hdr(out, outsize, 0x19, 2);
    if (n < 0) return n;
    out[4] = (nl_u8)(location >> 8); out[5] = (nl_u8)location;
    return 6;
}

long nl_cal_req_first_free(nl_u8 *out, long outsize)
{
    long n = req_hdr(out, outsize, 0x31, 0);
    return n < 0 ? n : 4;
}

/* ---- Writes (Phase 2) ----------------------------------------------- */

/* Write Mac Roman text as UCS-2 big-endian at out; returns the char count. */
static long cal_put_ucs2(nl_u8 *out, const char *s)
{
    long i;
    for (i = 0; s[i]; i++) {
        nl_u16 u = nl_macroman_char((nl_u8)s[i]);
        out[2*i] = (nl_u8)(u >> 8);
        out[2*i + 1] = (nl_u8)u;
    }
    return i;
}

long nl_cal_req_add(nl_u8 *out, long outsize, const nl_cal_note *n)
{
    long length, diff, refdays, notedays;
    int i, tl;

    if (n->type == NL_CAL_BIRTHDAY) return NL_CAL_ERR_TYPE;   /* not encoded yet */
    if (outsize < 64) return NL_CAL_ERR_SPACE;

    for (i = 0; i < 29; i++) out[i] = 0;
    out[0]=0x00; out[1]=0x01; out[2]=0x00;        /* N6110 header */
    out[3]=0x40;                                   /* add-note */
    out[13]=0x00; out[14]=0x00; out[15]=0xFF; out[16]=0xFF;   /* no alarm */
    out[18]=(nl_u8)n->type;

    /* Start time as a seconds difference from 2030-01-01, with the note's
     * year shifted back 20 (the phone's "method 2" scheme). */
    refdays  = nl_date_to_days(nl_date_make(2030, 1, 1));
    notedays = nl_date_to_days(nl_date_make(nl_date_year(n->date) - 20,
                                            nl_date_month(n->date), nl_date_day(n->date)));
    diff = (notedays - refdays) * 86400L + (long)n->hour * 3600L + (long)n->minute * 60L;
    out[9]=(nl_u8)(diff >> 24); out[10]=(nl_u8)(diff >> 16);
    out[11]=(nl_u8)(diff >> 8); out[12]=(nl_u8)diff;

    if (n->type == NL_CAL_CALL) {
        int numl = (int)strlen(n->number);
        tl = (int)strlen(n->text);
        if (27 + numl*2 + tl*2 + 2 > outsize) return NL_CAL_ERR_SPACE;
        out[25] = (nl_u8)numl;
        length = 27 + cal_put_ucs2(out + 27, n->number) * 2;
        out[26] = (nl_u8)tl;
        length += cal_put_ucs2(out + length, n->text) * 2;
    } else {
        /* meeting (0x01) or reminder/memo (0x08) */
        tl = (int)strlen(n->text);
        if (27 + tl*2 + 2 > outsize) return NL_CAL_ERR_SPACE;
        out[25] = (nl_u8)tl;
        out[26] = 0x00;
        length = 27 + cal_put_ucs2(out + 27, n->text) * 2;
    }
    out[length++] = 0x00;
    out[length++] = 0x00;
    out[4] = out[17] = (nl_u8)(length - 7);
    return length;
}

long nl_cal_req_delete(nl_u8 *out, long outsize, nl_u16 location)
{
    if (outsize < 6) return NL_CAL_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00;        /* N6110 header */
    out[3]=0x0B;
    out[4]=(nl_u8)(location >> 8); out[5]=(nl_u8)location;
    return 6;
}

int nl_cal_parse_add(const nl_u8 *body, long len)
{
    if (len < 4) return NL_CAL_ERR_SHORT;
    return body[3] == NL_CAL_ADD_OK ? NL_CAL_OK : NL_CAL_ERR_SUB;
}

int nl_cal_parse_delete(const nl_u8 *body, long len)
{
    if (len < 4) return NL_CAL_ERR_SHORT;
    return body[3] == NL_CAL_DELETE_OK ? NL_CAL_OK : NL_CAL_ERR_SUB;
}

/* ---- Replies -------------------------------------------------------- */

int nl_cal_parse_notes_info(const nl_u8 *body, long len,
                            nl_u16 *total, nl_u16 *locs, int maxlocs, int *nlocs)
{
    int i = 0;
    if (nlocs) *nlocs = 0;
    if (len < 8) return NL_CAL_ERR_SHORT;
    if (body[3] != 0x3B) return NL_CAL_ERR_SUB;
    if (total) *total = (nl_u16)u16(body + 4);
    /* Locations are 2 bytes each from offset 8, to the end of the frame. */
    while (8 + 2 * i + 1 < len) {
        nl_u16 loc = (nl_u16)u16(body + 8 + 2 * i);
        if (loc == 0) break;                 /* 0 ends the list */
        if (i >= maxlocs) return NL_CAL_ERR_SPACE;
        if (locs) locs[i] = loc;
        i++;
    }
    if (nlocs) *nlocs = i;
    if (i == 0) return NL_CAL_ERR_EMPTY;
    return NL_CAL_OK;
}

int nl_cal_parse_first_free(const nl_u8 *body, long len, nl_u16 *location)
{
    if (len < 6) return NL_CAL_ERR_SHORT;
    if (body[3] != 0x32) return NL_CAL_ERR_SUB;
    if (location) *location = (nl_u16)u16(body + 4);
    return NL_CAL_OK;
}

static nl_cal_recur recur_from_hours(int h)
{
    switch (h) {
    case 0:      return NL_REC_NONE;
    case 24:     return NL_REC_DAILY;
    case 168:    return NL_REC_WEEKLY;
    case 336:    return NL_REC_FORTNIGHTLY;
    case 0xFFFF: return NL_REC_YEARLY;
    default:     return NL_REC_OTHER;
    }
}

/* Copy a UCS-2 text of `chars` characters at body+off into dst (Mac Roman).
 * Returns NL_CAL_OK or an error; needs the whole text to be within len. */
static int copy_text(const nl_u8 *body, long len, long off, int chars,
                     char *dst, long dstsize, long *lost)
{
    long got;
    if (chars < 0) return NL_CAL_ERR_SHORT;
    if (off + 2L * chars > len) return NL_CAL_ERR_SHORT;
    got = nl_ucs2_to_macroman(body + off, chars, dst, dstsize, lost);
    return got < 0 ? NL_CAL_ERR_SPACE : NL_CAL_OK;
}

int nl_cal_parse_note(const nl_u8 *body, long len, nl_cal_note *note)
{
    int type, r;
    long off;

    if (len < 14) return NL_CAL_ERR_SHORT;
    if (body[3] != 0x1A) return NL_CAL_ERR_SUB;

    /* clear */
    note->location = (nl_u16)u16(body + 4);
    note->date = 0; note->hour = note->minute = 0;
    note->has_alarm = 0; note->alarm_before = 0; note->alarm_silent = 0;
    note->recur = NL_REC_NONE; note->recur_hours = 0; note->birth_year = 0;
    note->text[0] = 0; note->text_lost = 0;
    note->number[0] = 0; note->number_lost = 0;

    type = body[6];
    note->type = (nl_cal_type)type;

    if (type != NL_CAL_BIRTHDAY) {
        int y = (int)u16(body + 8), mo = body[10], da = body[11];
        note->date = nl_date_make(y, mo, da);   /* 0 if the phone gave an odd value */
    }

    switch (type) {
    case NL_CAL_MEETING: {
        int alarm, textlen;
        if (len < 20) return NL_CAL_ERR_SHORT;
        note->hour = body[12]; note->minute = body[13];
        alarm = (int)u16(body + 14);
        if (alarm != 0xFFFF) { note->has_alarm = 1; note->alarm_before = alarm; }
        note->recur_hours = (int)u16(body + 16);
        note->recur = recur_from_hours(note->recur_hours);
        textlen = body[18];
        r = copy_text(body, len, 20, textlen, note->text, NL_CAL_TEXT_MAX, &note->text_lost);
        return r;
    }
    case NL_CAL_CALL: {
        int alarm, textlen, numlen;
        if (len < 20) return NL_CAL_ERR_SHORT;
        note->hour = body[12]; note->minute = body[13];
        alarm = (int)u16(body + 14);
        if (alarm != 0xFFFF) { note->has_alarm = 1; note->alarm_before = alarm; }
        note->recur_hours = (int)u16(body + 16);
        note->recur = recur_from_hours(note->recur_hours);
        textlen = body[18]; numlen = body[19];
        r = copy_text(body, len, 20, textlen, note->text, NL_CAL_TEXT_MAX, &note->text_lost);
        if (r != NL_CAL_OK) return r;
        off = 20 + 2L * textlen;
        return copy_text(body, len, off, numlen, note->number, NL_CAL_NUM_MAX, &note->number_lost);
    }
    case NL_CAL_REMINDER: {
        int textlen;
        if (len < 16) return NL_CAL_ERR_SHORT;
        note->recur_hours = (int)u16(body + 12);
        note->recur = recur_from_hours(note->recur_hours);
        textlen = body[14];
        return copy_text(body, len, 16, textlen, note->text, NL_CAL_TEXT_MAX, &note->text_lost);
    }
    case NL_CAL_BIRTHDAY: {
        long alarm;
        int textlen, by;
        if (len < 22) return NL_CAL_ERR_SHORT;
        /* month and day are the birthday; year here is not the note year */
        note->date = nl_date_make(2000, body[10], body[11]);   /* a placeholder year for month/day */
        if (note->date == 0) note->date = 0;
        note->hour = 23; note->minute = 59;
        alarm = ((long)body[14] << 24) | ((long)body[15] << 16)
              | ((long)body[16] << 8) | body[17];
        if (alarm != 0x0000FFFFL) {
            note->has_alarm = 1; note->alarm_before = alarm;
            note->alarm_silent = (body[20] != 0);
        }
        by = (int)u16(body + 18);
        note->birth_year = (by == 0xFFFF) ? 0 : by;
        note->recur = NL_REC_YEARLY; note->recur_hours = 0xFFFF;
        textlen = body[21];
        return copy_text(body, len, 22, textlen, note->text, NL_CAL_TEXT_MAX, &note->text_lost);
    }
    default:
        return NL_CAL_ERR_TYPE;
    }
}

const char *nl_cal_type_name(nl_cal_type t)
{
    switch (t) {
    case NL_CAL_MEETING:  return "Meeting";
    case NL_CAL_CALL:     return "Call";
    case NL_CAL_BIRTHDAY: return "Birthday";
    case NL_CAL_REMINDER: return "Reminder";
    default:              return "?";
    }
}

const char *nl_cal_recur_name(nl_cal_recur r)
{
    switch (r) {
    case NL_REC_NONE:        return "none";
    case NL_REC_DAILY:       return "daily";
    case NL_REC_WEEKLY:      return "weekly";
    case NL_REC_FORTNIGHTLY: return "fortnightly";
    case NL_REC_YEARLY:      return "yearly";
    default:                 return "other";
    }
}

/* Text messages over the cable (see nlsms.h). */
#include "nlsms.h"

/* Field offsets within the message frame (which starts at body+9).
 * Deliver and submit lay their fields in fixed slots (gnokii's layout). */
typedef struct {
    int number, smsc, dcs, udl, firstbyte, time, text;
} sms_layout;

/* 255 means "not in this kind of message". */
static const sms_layout LAY_DELIVER = { 16, 0, 14, 15, 12, 28, 35 };
static const sms_layout LAY_SUBMIT  = { 17, 0, 15, 16, 12, 255, 36 };

static long u16(const nl_u8 *p) { return (p[0] << 8) | p[1]; }

long nl_sms_req_status(nl_u8 *out, long outsize)
{
    if (outsize < 5) return NL_SMS_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00; out[3]=0x36; out[4]=0x64;
    return 5;
}

long nl_sms_req_read(nl_u8 *out, long outsize, nl_u8 folder, nl_u16 location)
{
    if (outsize < 7) return NL_SMS_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00; out[3]=0x07;
    out[4]=folder; out[5]=(nl_u8)(location >> 8); out[6]=(nl_u8)location;
    return 7;
}

long nl_sms_req_folders(nl_u8 *out, long outsize)
{
    if (outsize < 6) return NL_SMS_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00; out[3]=0x7A; out[4]=0x00; out[5]=0x00;
    return 6;
}

long nl_sms_req_folder_status(nl_u8 *out, long outsize, nl_u8 folderid)
{
    if (outsize < 7) return NL_SMS_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x01;   /* N7110 header */
    out[3]=0x6B; out[4]=folderid; out[5]=0x0F; out[6]=0x01;
    return 7;
}

int nl_sms_parse_folders(const nl_u8 *body, long len, nl_sms_folder *out, int max, int *n)
{
    int count, i;
    long cur = 5;
    *n = 0;
    if (len < 5) return NL_SMS_ERR_SHORT;
    if (body[3] != 0x7B) return NL_SMS_ERR_TYPE;
    count = body[4];
    for (i = 0; i < count && i < max; i++) {
        long namestart, chars;
        long lost = 0;
        if (cur >= len) return NL_SMS_ERR_SHORT;
        out[i].id = body[cur++];
        namestart = cur;
        while (cur + 1 < len && (body[cur] || body[cur + 1])) cur += 2;
        chars = (cur - namestart) / 2;
        nl_ucs2_to_macroman(body + namestart, (int)chars, out[i].name,
                            (long)sizeof out[i].name, &lost);
        out[i].name_lost = lost;
        cur += 2;    /* skip the UCS-2 null terminator */
        (*n)++;
    }
    return NL_SMS_OK;
}

int nl_sms_parse_folder_status(const nl_u8 *body, long len, nl_u16 *locs, int max, int *n)
{
    long count, i;
    *n = 0;
    if (len < 6) return NL_SMS_ERR_SHORT;
    if (body[3] != 0x6C) return NL_SMS_ERR_TYPE;
    count = (long)body[4] * 256 + body[5];
    for (i = 0; i < count && *n < max; i++) {
        if (6 + i * 2 + 1 >= len) break;
        locs[(*n)++] = (nl_u16)(body[6 + i * 2] * 256 + body[7 + i * 2]);
    }
    return NL_SMS_OK;
}

long nl_sms_req_smsc(nl_u8 *out, long outsize, nl_u8 location)
{
    if (outsize < 6) return NL_SMS_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00;   /* N6110 header */
    out[3]=0x33; out[4]=0x64; out[5]=location;
    return 6;
}

long nl_sms_req_delete(nl_u8 *out, long outsize, nl_u8 folderid, nl_u16 location)
{
    if (outsize < 8) return NL_SMS_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x01;   /* N7110 header */
    out[3]=0x0A; out[4]=folderid;
    out[5]=(nl_u8)(location >> 8); out[6]=(nl_u8)location; out[7]=0x01;
    return 8;
}

int nl_sms_parse_smsc(const nl_u8 *body, long len,
                      char *number, long numsize, char *name, long namesize)
{
    long i, n;
    if (numsize > 0) number[0] = 0;
    if (namesize > 0) name[0] = 0;
    if (len < 22) return NL_SMS_ERR_SHORT;
    if (body[3] != 0x34) return NL_SMS_ERR_TYPE;   /* 0x35 = no SMSC at this location */
    /* The centre's number: a service-centre address (octet count incl TOA). */
    if (nl_gsm_number(body + 21, len - 21, 0, number, numsize) < 0 && numsize > 0)
        number[0] = 0;
    /* The name: ASCII from offset 33, zero-terminated. */
    for (i = 33, n = 0; i < len && body[i] && n + 1 < namesize; i++) name[n++] = (char)body[i];
    if (namesize > 0) name[n] = 0;
    return NL_SMS_OK;
}

int nl_sms_parse_status(const nl_u8 *body, long len, nl_sms_status *st)
{
    if (len < 18) return NL_SMS_ERR_SHORT;
    if (body[3] != 0x37) return NL_SMS_ERR_TYPE;
    st->sim_size     = (nl_u16)u16(body + 8);
    st->phone_used   = (nl_u16)u16(body + 10);
    st->phone_unread = (nl_u16)u16(body + 12);
    st->sim_used     = (nl_u16)u16(body + 14);
    st->sim_unread   = (nl_u16)u16(body + 16);
    return NL_SMS_OK;
}

/* Decode the message text in whichever coding it uses, into Mac Roman.
 * When the message carries a user-data header (udh), the header sits at the
 * start of the user data and must be skipped: for 7-bit it occupies whole
 * septets with fill bits so the text stays septet-aligned; for 8-bit/UCS-2
 * it is a whole number of octets. The header length byte (UDHL) counts the
 * header after itself, so the header is UDHL+1 octets in all. */
static int decode_text(const nl_u8 *frame, long flen, const sms_layout *L,
                       int udl, int udh, nl_sms_coding coding, char *out, long outsize, long *lost)
{
    long toff = L->text;
    int udh_oct = 0;
    *lost = 0;
    out[0] = 0;
    if (udh) {
        if (toff >= flen) return NL_SMS_ERR_SHORT;
        udh_oct = frame[toff] + 1;
    }
    if (coding == NL_SMS_7BIT) {
        int fill = 0, septets = udl;
        long need;
        if (udh) {
            long n = (long)udh_oct * 8;
            int udh_septets = (int)((n + 6) / 7);   /* septets the header fills */
            fill = udh_septets * 7 - (int)n;        /* 0..6 padding bits after it */
            septets = udl - udh_septets;
            toff += udh_oct;                         /* header is a whole octet count */
        }
        if (septets < 0) return NL_SMS_ERR_SHORT;
        need = (septets * 7 + fill + 7) / 8;
        if (toff + need > flen) return NL_SMS_ERR_SHORT;
        if (nl_gsm7_to_macroman(frame + toff, flen - toff, fill, septets, out, outsize, lost) < 0)
            return NL_SMS_ERR_SPACE;
        return NL_SMS_OK;
    }
    /* 8-bit and UCS-2: the length is in octets; skip the header octets. */
    {
        long bytes = udl;
        if (udh) { toff += udh_oct; bytes = udl - udh_oct; }
        if (bytes < 0) return NL_SMS_ERR_SHORT;
        if (toff + bytes > flen) return NL_SMS_ERR_SHORT;
        if (coding == NL_SMS_UCS2) {
            if (nl_ucs2_to_macroman(frame + toff, (int)(bytes / 2), out, outsize, lost) < 0)
                return NL_SMS_ERR_SPACE;
            return NL_SMS_OK;
        }
        {   /* 8-bit: each byte is a Latin-1 character (Unicode 0..255). */
            long i;
            for (i = 0; i < bytes; i++) {
                nl_u8 be[2]; long one; char c[2];
                be[0] = 0; be[1] = frame[toff + i];
                one = nl_ucs2_to_macroman(be, 1, c, sizeof c, lost);
                if (one < 0 || i + 1 >= outsize) return NL_SMS_ERR_SPACE;
                out[i] = c[0];
            }
            out[bytes] = 0;
            return NL_SMS_OK;
        }
    }
}

/* Read the concatenation element from the user-data header at frame+toff
 * (which begins with the header length UDHL). Concatenation is IEI 0x00 (an
 * 8-bit reference) or 0x08 (16-bit); other elements are skipped. */
static void parse_concat_udh(const nl_u8 *frame, long flen, long toff, nl_sms *out)
{
    long udhl, p, end;
    if (toff >= flen) return;
    udhl = frame[toff];
    p = toff + 1;
    end = toff + 1 + udhl;
    if (end > flen) end = flen;
    while (p + 2 <= end) {
        int iei = frame[p], iedl = frame[p + 1];
        if (p + 2 + iedl > end) break;
        if (iei == 0x00 && iedl == 3) {
            out->concat = 1; out->concat_ref = frame[p + 2];
            out->concat_total = frame[p + 3]; out->concat_seq = frame[p + 4];
        } else if (iei == 0x08 && iedl == 4) {
            out->concat = 1; out->concat_ref = (frame[p + 2] << 8) | frame[p + 3];
            out->concat_total = frame[p + 4]; out->concat_seq = frame[p + 5];
        }
        p += 2 + iedl;
    }
}

int nl_sms_parse_message(const nl_u8 *body, long len, nl_sms *out)
{
    const nl_u8 *frame;
    long flen;
    const sms_layout *L;
    int firstbyte, dcs, udl, r;

    out->number[0] = 0; out->number_lost = 0; out->smsc[0] = 0;
    out->text[0] = 0; out->text_lost = 0;
    out->has_time = 0; out->tz_quarters = 0; out->udh = 0;
    out->time.date = 0; out->time.secs = 0;
    out->concat = 0; out->concat_ref = 0; out->concat_total = 0; out->concat_seq = 0;

    if (len < 9 + 13) return NL_SMS_ERR_SHORT;
    out->state = body[4];
    frame = body + 9;
    flen = len - 9;

    firstbyte = frame[12];
    out->udh = (firstbyte & 0x40) != 0;
    switch (firstbyte & 0x03) {
    case 0x00: out->pdu = NL_SMS_DELIVER; L = &LAY_DELIVER; break;
    case 0x01: out->pdu = NL_SMS_SUBMIT;  L = &LAY_SUBMIT;  break;
    case 0x02: out->pdu = NL_SMS_REPORT;  L = &LAY_DELIVER; break;
    default:   return NL_SMS_ERR_TYPE;
    }

    if (out->udh) parse_concat_udh(frame, flen, L->text, out);

    dcs = frame[L->dcs];
    switch ((dcs >> 2) & 0x03) {
    case 1:  out->coding = NL_SMS_8BIT; break;
    case 2:  out->coding = NL_SMS_UCS2; break;
    default: out->coding = NL_SMS_7BIT; break;
    }

    /* service centre and the other party's number */
    if (nl_gsm_number(frame + L->smsc, flen - L->smsc, 0, out->smsc, NL_SMS_NUM_MAX) < 0)
        out->smsc[0] = 0;
    if (L->number + 2 <= flen) {
        if (nl_gsm_number(frame + L->number, flen - L->number, 1,
                          out->number, NL_SMS_NUM_MAX) < 0)
            out->number[0] = 0;
    }

    /* time (deliver only) */
    if (L->time != 255 && L->time + 7 <= flen) {
        if (nl_gsm_timestamp(frame + L->time, flen - L->time, &out->time, &out->tz_quarters) == NL_GSM_OK)
            out->has_time = 1;
    }

    /* text */
    if (out->pdu == NL_SMS_REPORT) return NL_SMS_OK;   /* no user text */
    if (L->udl >= flen) return NL_SMS_ERR_SHORT;
    udl = frame[L->udl];
    r = decode_text(frame, flen, L, udl, out->udh, out->coding, out->text, NL_SMS_TEXT_MAX, &out->text_lost);
    return r;
}

const char *nl_sms_pdu_name(nl_sms_pdu p)
{
    switch (p) {
    case NL_SMS_DELIVER: return "received";
    case NL_SMS_SUBMIT:  return "sent";
    case NL_SMS_REPORT:  return "delivery report";
    default:             return "?";
    }
}

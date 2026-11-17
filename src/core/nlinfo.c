/* Phone status and clock (see nlinfo.h). */
#include "nlinfo.h"

static long u16(const nl_u8 *p) { return (p[0] << 8) | p[1]; }

/* ---- Requests ------------------------------------------------------- */

static long body4(nl_u8 *out, long outsize, nl_u8 sub)
{
    if (outsize < 4) return NL_INFO_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00; out[3]=sub;
    return 4;
}

long nl_info_req_identify(nl_u8 *out, long outsize)
{
    if (outsize < 5) return NL_INFO_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x00; out[3]=0x03; out[4]=0x00;
    return 5;
}
long nl_info_req_imei(nl_u8 *out, long outsize)
{
    if (outsize < 4) return NL_INFO_ERR_SPACE;
    out[0]=0x00; out[1]=0x01; out[2]=0x66; out[3]=0x00;   /* note: 0x66, not 0x00 */
    return 4;
}
long nl_info_req_battery(nl_u8 *out, long outsize) { return body4(out, outsize, 0x02); }
long nl_info_req_signal(nl_u8 *out, long outsize)  { return body4(out, outsize, 0x81); }
long nl_info_req_network(nl_u8 *out, long outsize) { return body4(out, outsize, 0x70); }
long nl_info_req_clock(nl_u8 *out, long outsize)   { return body4(out, outsize, 0x62); }

/* ---- Replies -------------------------------------------------------- */

/* Copy one text line (printable ASCII, stops at CR/LF/zero/end). */
static void copy_line(const nl_u8 *p, long n, char *out, long outsize)
{
    long i = 0;
    while (i < n && p[i] >= 0x20 && p[i] < 0x7F && i + 1 < outsize) { out[i] = (char)p[i]; i++; }
    out[i] = 0;
}

int nl_info_parse_identify(const nl_u8 *body, long len, nl_info_ident *out)
{
    /* The reply is ASCII text in several lines (version, date, model,
     * copyright). Pick each by shape, so the exact offset need not be known. */
    long i = 0, start;
    out->model[0] = out->version[0] = out->date[0] = 0;
    while (i < len) {
        if (body[i] < 0x20 || body[i] >= 0x7F) { i++; continue; }
        start = i;
        while (i < len && body[i] >= 0x20 && body[i] < 0x7F) i++;
        {
            char line[32];
            long n = i - start;
            copy_line(body + start, n, line, sizeof line);
            /* model: letters-letters-digit, e.g. NSE-5 */
            if (!out->model[0]) {
                int a=0,j; for (j=0; line[j]; j++) a++;
                if (a >= 4 && line[3] == '-' &&
                    line[0]>='A'&&line[0]<='Z' && line[1]>='A'&&line[1]<='Z' &&
                    line[2]>='A'&&line[2]<='Z' && line[4]>='0'&&line[4]<='9') {
                    long k; for (k=0;line[k]&&k<15;k++) out->model[k]=line[k]; out->model[k]=0;
                }
            }
            /* version: a line starting with 'V' then space/digit */
            if (!out->version[0] && line[0]=='V' && (line[1]==' '||((line[1]>='0'&&line[1]<='9')))) {
                const char *v = line + 1; while (*v==' ') v++;
                long k; for (k=0; v[k] && k<23; k++) out->version[k]=v[k]; out->version[k]=0;
            }
            /* date: dd-dd-dd */
            if (!out->date[0] && line[0]>='0'&&line[0]<='9' && line[1]>='0'&&line[1]<='9'
                && line[2]=='-' && line[5]=='-') {
                long k; for (k=0; line[k] && k<15; k++) out->date[k]=line[k]; out->date[k]=0;
            }
        }
    }
    return out->model[0] ? NL_INFO_OK : NL_INFO_ERR_NODATA;
}

int nl_info_parse_imei(const nl_u8 *body, long len, char *out, long outsize)
{
    long i;
    if (len < 5) return NL_INFO_ERR_SHORT;
    for (i = 0; i + 4 < len && body[4 + i] >= '0' && body[4 + i] <= '9'; i++) {
        if (i + 1 >= outsize) return NL_INFO_ERR_SPACE;
        out[i] = (char)body[4 + i];
    }
    out[i] = 0;
    return i ? NL_INFO_OK : NL_INFO_ERR_NODATA;
}

int nl_info_parse_battery(const nl_u8 *body, long len, int *percent)
{
    if (len < 6) return NL_INFO_ERR_SHORT;
    if (percent) *percent = body[5];
    return NL_INFO_OK;
}

int nl_info_parse_signal(const nl_u8 *body, long len, int *percent)
{
    if (len < 5) return NL_INFO_ERR_SHORT;
    if (body[3] != 0x82) return NL_INFO_ERR_SUB;
    if (percent) *percent = body[4];
    return NL_INFO_OK;
}

/* 3 BCD bytes to "MCC MNC" (the GSM PLMN encoding). */
static void network_code(const nl_u8 *p, char *out)
{
    int mcc1=p[0]&0xF, mcc2=p[0]>>4, mcc3=p[1]&0xF;
    int mnc3=p[1]>>4, mnc1=p[2]&0xF, mnc2=p[2]>>4;
    int n = 0;
    out[n++]=(char)('0'+mcc1); out[n++]=(char)('0'+mcc2); out[n++]=(char)('0'+mcc3);
    out[n++]=' ';
    out[n++]=(char)('0'+mnc1); out[n++]=(char)('0'+mnc2);
    if (mnc3 != 0xF) out[n++]=(char)('0'+mnc3);
    out[n]=0;
}

static void hex2(const nl_u8 *p, char *out)
{
    static const char H[]="0123456789ABCDEF";
    out[0]=H[p[0]>>4]; out[1]=H[p[0]&0xF]; out[2]=H[p[1]>>4]; out[3]=H[p[1]&0xF]; out[4]=0;
}

int nl_info_parse_network(const nl_u8 *body, long len, nl_info_network *out)
{
    int status;
    out->registered = 0; out->code[0]=0; out->lac[0]=0; out->cid[0]=0;
    out->name[0]=0; out->name_lost=0;
    if (len < 18) return NL_INFO_ERR_SHORT;
    if (body[3] != 0x71) return NL_INFO_ERR_SUB;
    status = body[8];
    out->registered = (status < 0x03);
    if (out->registered) {
        hex2(body + 10, out->cid);
        hex2(body + 12, out->lac);
        network_code(body + 14, out->code);
        {
            int namebytes = body[17];      /* name is UCS-2, body[17] characters */
            if (18 + namebytes * 2 <= len)
                nl_ucs2_to_macroman(body + 18, namebytes, out->name, sizeof out->name, &out->name_lost);
        }
    }
    return NL_INFO_OK;
}

int nl_info_parse_clock(const nl_u8 *body, long len, nl_stamp *out)
{
    int y, mo, da, hh, mi, se;
    if (len < 15) return NL_INFO_ERR_SHORT;
    if (body[3] != 0x63) return NL_INFO_ERR_SUB;
    if (body[4] != 0x01) return NL_INFO_ERR_NODATA;   /* clock not available */
    y = (int)u16(body + 8); mo = body[10]; da = body[11];
    hh = body[12]; mi = body[13]; se = body[14];
    out->date = nl_date_make(y, mo, da);
    out->secs = (long)hh * 3600 + (long)mi * 60 + se;
    if (out->date == 0) return NL_INFO_ERR_NODATA;
    return NL_INFO_OK;
}

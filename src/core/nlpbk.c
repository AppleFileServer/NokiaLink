/* Contacts over the cable (see nlpbk.h). */
#include "nlpbk.h"

/* Sub-block field ids (gnokii ncommon.h). */
#define PBK_NAME    0x07
#define PBK_EMAIL   0x08
#define PBK_POSTAL  0x09
#define PBK_NOTE    0x0A
#define PBK_NUMBER  0x0B
#define PBK_DATETIME 0x13

/* Number sub-types. */
#define NUM_HOME    0x02
#define NUM_MOBILE  0x03
#define NUM_FAX     0x04
#define NUM_WORK    0x06

static long u16(const nl_u8 *p) { return (p[0] << 8) | p[1]; }

static int valid_mem(nl_u8 m)
{
    return m == NL_PBK_MEM_ME || m == NL_PBK_MEM_SIM ||
           m == NL_PBK_MEM_DIALLED || m == NL_PBK_MEM_MISSED || m == NL_PBK_MEM_RECEIVED;
}

/* ---- Requests ------------------------------------------------------- */

long nl_pbk_req_read(nl_u8 *out, long outsize, nl_u8 memtype, nl_u16 location)
{
    if (!valid_mem(memtype)) return NL_PBK_ERR_MEM;
    if (outsize < 14) return NL_PBK_ERR_SPACE;
    out[0] = 0x00; out[1] = 0x01; out[2] = 0x01;   /* N7110 header */
    out[3] = 0x07; out[4] = 0x01; out[5] = 0x01; out[6] = 0x00;
    out[7] = 0x01; out[8] = 0x02;
    out[9] = memtype;
    out[10] = (nl_u8)(location >> 8); out[11] = (nl_u8)location;
    out[12] = 0x00; out[13] = 0x00;
    return 14;
}

long nl_pbk_req_status(nl_u8 *out, long outsize, nl_u8 memtype)
{
    if (!valid_mem(memtype)) return NL_PBK_ERR_MEM;
    if (outsize < 6) return NL_PBK_ERR_SPACE;
    out[0] = 0x00; out[1] = 0x01; out[2] = 0x00;   /* N6110 header */
    out[3] = 0x03; out[4] = 0x02; out[5] = memtype;
    return 6;
}

/* ---- Replies -------------------------------------------------------- */

int nl_pbk_parse_status(const nl_u8 *body, long len, nl_u16 *used, nl_u16 *total)
{
    long cap;
    if (len < 18) return NL_PBK_ERR_SHORT;
    /* capacity: a 2-byte value at 14 when byte 10 is 0x10, else a byte at 18 */
    if (body[10] == 0x10) cap = u16(body + 14);
    else cap = body[18];
    if (used)  *used  = (nl_u16)u16(body + 16);
    if (total) *total = (nl_u16)cap;
    return NL_PBK_OK;
}

/* Convert a UCS-2 text of `bytes` bytes (chars*2) at body+off to Mac
 * Roman in dst. Guards against running past the body. */
static int copy_text(const nl_u8 *body, long len, long off, int bytes,
                     char *dst, long dstsize, long *lost)
{
    int chars, i;
    if (bytes < 0 || off + bytes > len) return NL_PBK_ERR_SHORT;
    chars = bytes / 2;
    /* The phone NUL-terminates each text and counts the terminator (and any
     * padding) in the length; stop at the first UCS-2 NUL so the terminator
     * is not decoded as a character. */
    for (i = 0; i < chars; i++)
        if (body[off + 2*i] == 0 && body[off + 2*i + 1] == 0) { chars = i; break; }
    if (nl_ucs2_to_macroman(body + off, chars, dst, dstsize, lost) < 0)
        return NL_PBK_ERR_SPACE;
    return NL_PBK_OK;
}

static nl_num_type num_type(nl_u8 b)
{
    switch (b) {
    case NUM_HOME:   return NL_NUM_HOME;
    case NUM_MOBILE: return NL_NUM_MOBILE;
    case NUM_FAX:    return NL_NUM_FAX;
    case NUM_WORK:   return NL_NUM_WORK;
    default:         return NL_NUM_GENERAL;   /* 0x00/0x01/0x08/0x0A/0x0B/0x55/... */
    }
}

int nl_pbk_parse_entry(const nl_u8 *body, long len, nl_u16 location, nl_pbk_entry *e)
{
    long off;
    int i;

    /* clear */
    e->location = location;
    e->name[0] = 0; e->name_lost = 0;
    e->email[0] = 0; e->email_lost = 0;
    e->postal[0] = 0; e->postal_lost = 0;
    e->note[0] = 0; e->note_lost = 0;
    e->nnumbers = 0;
    e->has_when = 0; e->when.date = 0; e->when.secs = 0;
    for (i = 0; i < NL_PBK_MAX_NUMBERS; i++) { e->numbers[i].text[0] = 0; e->numbers[i].lost = 0; }

    if (len < 11) return NL_PBK_ERR_SHORT;
    /* An error subframe (no entry at this location). */
    if (body[6] == 0x0F) return NL_PBK_ERR_EMPTY;
    if (len < 18) return NL_PBK_ERR_SHORT;

    /* Sub-blocks start at offset 18 and run to the end. */
    off = 18;
    while (off + 6 <= len) {
        nl_u8 id = body[off];
        long bs = 256L * body[off + 2] + body[off + 3];
        int r;
        if (bs < 6 || off + bs > len) break;    /* malformed or past the end */

        switch (id) {
        case PBK_NAME:
            r = copy_text(body, len, off + 6, body[off + 5],
                          e->name, NL_PBK_TEXT_MAX, &e->name_lost);
            if (r != NL_PBK_OK) return r;
            break;
        case PBK_EMAIL:
            r = copy_text(body, len, off + 6, body[off + 5],
                          e->email, NL_PBK_TEXT_MAX, &e->email_lost);
            if (r != NL_PBK_OK) return r;
            break;
        case PBK_POSTAL:
            r = copy_text(body, len, off + 6, body[off + 5],
                          e->postal, NL_PBK_TEXT_MAX, &e->postal_lost);
            if (r != NL_PBK_OK) return r;
            break;
        case PBK_NOTE:
            r = copy_text(body, len, off + 6, body[off + 5],
                          e->note, NL_PBK_TEXT_MAX, &e->note_lost);
            if (r != NL_PBK_OK) return r;
            break;
        case PBK_NUMBER:
            if (off + 10 > len) break;
            if (e->nnumbers < NL_PBK_MAX_NUMBERS) {
                nl_pbk_number *num = &e->numbers[e->nnumbers];
                num->type = num_type(body[off + 5]);
                r = copy_text(body, len, off + 10, body[off + 9],
                              num->text, NL_PBK_NUM_MAX, &num->lost);
                if (r != NL_PBK_OK) return r;
                e->nnumbers++;
            }
            break;
        case PBK_DATETIME:
            if (off + 6 + 7 <= len) {
                const nl_u8 *d = body + off + 6;
                e->when.date = nl_date_make((int)((d[0] << 8) | d[1]), d[2], d[3]);
                e->when.secs = (long)d[4] * 3600 + (long)d[5] * 60 + d[6];
                e->has_when = (e->when.date != 0);
            }
            break;
        default:
            break;      /* ringtone id, caller group etc: ignored for now */
        }
        off += bs;
    }
    return NL_PBK_OK;
}

const char *nl_num_type_name(nl_num_type t)
{
    switch (t) {
    case NL_NUM_HOME:   return "home";
    case NL_NUM_MOBILE: return "mobile";
    case NL_NUM_FAX:    return "fax";
    case NL_NUM_WORK:   return "work";
    default:            return "general";
    }
}

const char *nl_pbk_mem_name(nl_u8 memtype)
{
    switch (memtype) {
    case NL_PBK_MEM_ME:       return "phone";
    case NL_PBK_MEM_SIM:      return "SIM";
    case NL_PBK_MEM_DIALLED:  return "dialled";
    case NL_PBK_MEM_MISSED:   return "missed";
    case NL_PBK_MEM_RECEIVED: return "received";
    default:                  return "?";
    }
}

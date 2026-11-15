/* FBUS version 2 framing. See fbus.h and docs/FBUS.md. */
#include "fbus.h"

/* ---- Encoding ---------------------------------------------------- */

long nl_fbus_encode_raw(nl_u8 *out, nl_u8 dest, nl_u8 src, nl_u8 type,
                        const nl_u8 *block, long len)
{
    long n = 0, i;
    nl_u8 even = 0, odd = 0;

    out[n++] = NL_FBUS_FRAME_ID;
    out[n++] = dest;
    out[n++] = src;
    out[n++] = type;
    out[n++] = (nl_u8)((len >> 8) & 0xFF);
    out[n++] = (nl_u8)(len & 0xFF);
    for (i = 0; i < len; i++)
        out[n++] = block[i];
    if (len & 1)
        out[n++] = 0x00;

    /* n is even here, so the first checksum lands on an even offset. */
    for (i = 0; i < n; i += 2) {
        even ^= out[i];
        odd  ^= out[i + 1];
    }
    out[n++] = even;
    out[n++] = odd;
    return n;
}

long nl_fbus_encode_data(nl_u8 *out, nl_u8 type, const nl_u8 *data, long len,
                         nl_u8 frames_to_go, nl_u8 seq)
{
    nl_u8 block[NL_FBUS_CHUNK + 2];
    long i;

    if (len > NL_FBUS_CHUNK)
        len = NL_FBUS_CHUNK;
    for (i = 0; i < len; i++)
        block[i] = data[i];
    block[len]     = frames_to_go;
    block[len + 1] = seq;
    return nl_fbus_encode_raw(out, NL_FBUS_DEV_PHONE, NL_FBUS_DEV_PC, type,
                              block, len + 2);
}

long nl_fbus_encode_ack(nl_u8 *out, nl_u8 type, nl_u8 seq)
{
    nl_u8 block[2];

    block[0] = type;
    block[1] = seq;
    return nl_fbus_encode_raw(out, NL_FBUS_DEV_PHONE, NL_FBUS_DEV_PC,
                              NL_FBUS_TYPE_ACK, block, 2);
}

/* ---- Parser ------------------------------------------------------ */

enum { ST_SYNC, ST_DEST, ST_SRC, ST_TYPE, ST_LEN_HI, ST_LEN_LO, ST_BODY };

void nl_fbus_parser_reset(nl_fbus_parser *p)
{
    p->state = ST_SYNC;
    p->pos = 0;
    p->count = 0;
    p->need = 0;
    p->xor_even = 0;
    p->xor_odd = 0;
}

void nl_fbus_parser_init(nl_fbus_parser *p, nl_u8 want_dest, nl_u8 want_src)
{
    p->want_dest = want_dest;
    p->want_src = want_src;
    p->type = 0;
    p->length = 0;
    p->sum1 = p->sum2 = 0;
    p->good_frames = 0;
    p->bad_checksums = 0;
    p->too_long = 0;
    p->skipped_bytes = 0;
    nl_fbus_parser_reset(p);
}

/* A byte that can't continue the frame in hand. If it is itself a frame
 * marker, start a new frame with it; otherwise go back to looking. */
static void restart_with(nl_fbus_parser *p, nl_u8 b)
{
    nl_fbus_parser_reset(p);
    if (b == NL_FBUS_FRAME_ID) {
        p->xor_even = b;
        p->pos = 1;
        p->state = ST_DEST;
    } else {
        p->skipped_bytes++;
    }
}

int nl_fbus_parser_byte(nl_fbus_parser *p, nl_u8 b)
{
    if (p->state == ST_SYNC) {
        restart_with(p, b);
        return 0;
    }

    switch (p->state) {
    case ST_DEST:
        if (b != p->want_dest) { p->skipped_bytes++; restart_with(p, b); return 0; }
        break;
    case ST_SRC:
        if (b != p->want_src) { p->skipped_bytes += 2; restart_with(p, b); return 0; }
        break;
    default:
        break;
    }

    if (p->pos & 1) p->xor_odd ^= b; else p->xor_even ^= b;
    p->pos++;

    switch (p->state) {
    case ST_DEST:   p->state = ST_SRC;  return 0;
    case ST_SRC:    p->state = ST_TYPE; return 0;
    case ST_TYPE:   p->type = b; p->state = ST_LEN_HI; return 0;
    case ST_LEN_HI: p->length = (nl_u16)(b << 8); p->state = ST_LEN_LO; return 0;
    case ST_LEN_LO:
        p->length = (nl_u16)(p->length | b);
        if (p->length > NL_FBUS_MAX_BLOCK) {
            p->too_long++;
            nl_fbus_parser_reset(p);
            return 0;
        }
        p->need = (nl_u16)(p->length + (p->length & 1) + 2);
        p->count = 0;
        p->state = ST_BODY;
        return 0;
    case ST_BODY:
        p->block[p->count++] = b;
        if (p->count < p->need)
            return 0;
        /* The checksums are part of what was XORed, so both come to zero. */
        if (p->xor_even != 0 || p->xor_odd != 0) {
            p->bad_checksums++;
            nl_fbus_parser_reset(p);
            return 0;
        }
        p->sum1 = p->block[p->need - 2];
        p->sum2 = p->block[p->need - 1];
        p->good_frames++;
        nl_fbus_parser_reset(p);
        return 1;
    default:
        nl_fbus_parser_reset(p);
        return 0;
    }
}

/* ---- Assembler --------------------------------------------------- */

void nl_fbus_asm_init(nl_fbus_asm *a, nl_u8 *buf, long cap)
{
    a->buf = buf;
    a->cap = cap;
    a->len = 0;
    a->type = 0;
    a->expect = 0;
    a->overflow = 0;
    a->dropped_frames = 0;
    a->overflows = 0;
}

void nl_fbus_asm_frame(nl_fbus_asm *a, nl_u8 type, const nl_u8 *block,
                       long len, nl_fbus_event *ev)
{
    nl_u8 to_go, seq;
    long data_len, i;

    ev->flags = 0;
    ev->msg = 0;
    ev->msg_len = 0;

    if (type == NL_FBUS_TYPE_ACK) {
        if (len < 2) { ev->flags = NL_FB_DROPPED; a->dropped_frames++; return; }
        ev->flags = NL_FB_GOT_ACK;
        ev->ack_type = block[0];
        ev->ack_seq = block[len - 1];
        return;
    }

    if (len < 2) { ev->flags = NL_FB_DROPPED; a->dropped_frames++; return; }
    to_go = block[len - 2];
    seq = block[len - 1];
    data_len = len - 2;

    if (seq & NL_FBUS_SEQ_FIRST) {
        /* First frame: any unfinished message is abandoned. */
        a->len = 0;
        a->type = type;
        a->expect = to_go ? to_go : 1;
        a->overflow = 0;
    } else if (a->expect == 0 || a->type != type || a->expect != to_go) {
        /* A continuation that doesn't follow on. Not acknowledged, so
         * the phone sends it again or gives the message up. */
        ev->flags = NL_FB_DROPPED;
        a->dropped_frames++;
        return;
    }

    if (a->len + data_len > a->cap) {
        a->overflow = 1;
    } else {
        for (i = 0; i < data_len; i++)
            a->buf[a->len + i] = block[i];
        a->len += data_len;
    }
    a->expect--;

    if (type != NL_FBUS_TYPE_TRACE) {
        ev->flags |= NL_FB_SEND_ACK;
        ev->reply_type = type;
        ev->reply_seq = (nl_u8)(seq & 0x0F);
    }

    if (a->expect == 0) {
        if (a->overflow) {
            ev->flags |= NL_FB_OVERFLOW;
            a->overflows++;
        } else {
            ev->flags |= NL_FB_MESSAGE;
            ev->msg_type = type;
            ev->msg = a->buf;
            ev->msg_len = a->len;
        }
    }
}

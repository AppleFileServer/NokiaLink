/* The pretend phone. See fakephone.h. */
#include <stdio.h>
#include <string.h>
#include "fakephone.h"

static void queue(fakephone *fp, const nl_u8 *b, long n)
{
    long i;
    for (i = 0; i < n && fp->tail < FP_QUEUE; i++)
        fp->to_pc[fp->tail++] = b[i];
}

static void note(fakephone *fp, const char *s)
{
    if (fp->log[0]) strncat(fp->log, " ", sizeof fp->log - strlen(fp->log) - 1);
    strncat(fp->log, s, sizeof fp->log - strlen(fp->log) - 1);
}

void fp_send_message(fakephone *fp, nl_u8 type, const nl_u8 *data, long len,
                     long chunk, int repeat_frames, int corrupt_first)
{
    nl_u8 block[NL_FBUS_MAX_BLOCK], frame[NL_FBUS_MAX_BLOCK + 16];
    long frames = (len + chunk - 1) / chunk, i, off = 0, part, n;

    if (frames < 1) frames = 1;
    for (i = 0; i < frames; i++) {
        part = len - off;
        if (part > chunk) part = chunk;
        memcpy(block, data + off, (size_t)part);
        block[part] = (nl_u8)(frames - i);
        block[part + 1] = (nl_u8)(fp->seq | (i == 0 ? NL_FBUS_SEQ_FIRST : 0));
        fp->seq = (nl_u8)((fp->seq + 1) & 7);
        n = nl_fbus_encode_raw(frame, NL_FBUS_DEV_PC, NL_FBUS_DEV_PHONE, type,
                               block, part + 2);
        if (corrupt_first && i == 0) {
            frame[7] ^= 0x10;
            queue(fp, frame, n);
            frame[7] ^= 0x10;
        }
        queue(fp, frame, n);
        if (repeat_frames) queue(fp, frame, n);
        off += part;
    }
}

static void phone_ack(fakephone *fp, nl_u8 type, nl_u8 seq)
{
    nl_u8 block[2], frame[16];
    long n;

    block[0] = type;
    block[1] = (nl_u8)(seq & 0x0F);
    n = nl_fbus_encode_raw(frame, NL_FBUS_DEV_PC, NL_FBUS_DEV_PHONE,
                           NL_FBUS_TYPE_ACK, block, 2);
    queue(fp, frame, n);
}

static void phone_frame(fakephone *fp)
{
    nl_fbus_parser *p = &fp->parser;
    nl_u8 to_go, seq;
    long data_len;
    int i;

    if (p->type == NL_FBUS_TYPE_ACK) {
        if (fp->acks_seen < FP_FRAMES) {
            fp->ack_type[fp->acks_seen] = p->block[0];
            fp->ack_seq[fp->acks_seen] = p->block[1];
        }
        fp->acks_seen++;
        return;
    }

    to_go = p->block[p->length - 2];
    seq = p->block[p->length - 1];
    data_len = p->length - 2;
    if (fp->frames_seen < FP_FRAMES) {
        fp->seen_to_go[fp->frames_seen] = to_go;
        fp->seen_seq[fp->frames_seen] = seq;
        fp->seen_len[fp->frames_seen] = data_len;
    }
    fp->frames_seen++;

    if (fp->ignore_frames > 0) { fp->ignore_frames--; return; }

    if (seq & NL_FBUS_SEQ_FIRST) fp->msg_len = 0;
    memcpy(fp->msg + fp->msg_len, p->block, (size_t)data_len);
    fp->msg_len += data_len;
    phone_ack(fp, p->type, seq);

    if (to_go != 1) return;
    fp->messages_seen++;
    if (fp->never_reply) return;
    for (i = 0; i < fp->n_replies; i++) {
        fp_reply *r = &fp->replies[i];
        if (r->req_type != p->type) continue;
        if (r->use_body) {
            nl_u16 got;
            if (fp->msg_len < r->body_off + 2) continue;
            got = (nl_u16)((fp->msg[r->body_off] << 8) | fp->msg[r->body_off + 1]);
            if (got != r->body_val) continue;
        }
        if (r->extra_before) {
            static const nl_u8 extra[] = { 0x01, 0x08, 0x00, 0x71 };
            fp_send_message(fp, 0x0A, extra, (long)sizeof extra, 100, 0, 0);
        }
        fp_send_message(fp, r->reply_type, r->data, r->len, r->chunk,
                        r->repeat_frames, r->corrupt_first);
        break;
    }
}

static long fp_write(void *ctx, const nl_u8 *bytes, long n)
{
    fakephone *fp = (fakephone *)ctx;
    long i;

    for (i = 0; i < n && fp->written_len < FP_QUEUE; i++)
        fp->written[fp->written_len++] = bytes[i];

    if (!fp->fbus) {
        if (fp->answer_at && n >= 4 && bytes[0] == 'A' && bytes[1] == 'T')
            queue(fp, (const nl_u8 *)"\r\nOK\r\n", 6);
        return n;
    }
    for (i = 0; i < n; i++)
        if (nl_fbus_parser_byte(&fp->parser, bytes[i]))
            phone_frame(fp);
    return n;
}

static long fp_read(void *ctx, nl_u8 *bytes, long max)
{
    fakephone *fp = (fakephone *)ctx;
    long n = 0;

    /* Hand bytes over a few at a time, as a serial port does. */
    if (max > 7) max = 7;
    while (n < max && fp->head < fp->tail)
        bytes[n++] = fp->to_pc[fp->head++];
    if (fp->head == fp->tail) fp->head = fp->tail = 0;
    return n;
}

static int fp_set_speed(void *ctx, long baud)
{
    fakephone *fp = (fakephone *)ctx;
    char s[24];

    snprintf(s, sizeof s, "S%ld", baud);
    note(fp, s);
    fp->fbus = (baud == 115200L);
    return 0;
}

static int fp_set_power(void *ctx, int on)
{
    note((fakephone *)ctx, on ? "P1" : "P0");
    return 0;
}

static nl_u32 fp_now(void *ctx)             { return ((fakephone *)ctx)->clock; }
static void fp_sleep(void *ctx, nl_u32 ms)  { ((fakephone *)ctx)->clock += ms; }

void fp_init(fakephone *fp)
{
    memset(fp, 0, sizeof *fp);
    fp->port.ctx = fp;
    fp->port.write = fp_write;
    fp->port.read = fp_read;
    fp->port.set_speed = fp_set_speed;
    fp->port.set_power = fp_set_power;
    fp->port.now_ms = fp_now;
    fp->port.sleep_ms = fp_sleep;
    fp->clock = 4000000000UL;   /* near the wrap, to test the arithmetic */
    fp->answer_at = 1;
    fp->fbus = 1;
    nl_fbus_parser_init(&fp->parser, NL_FBUS_DEV_PHONE, NL_FBUS_DEV_PC);
}

void fp_add_reply(fakephone *fp, const fp_reply *r)
{
    if (fp->n_replies < FP_REPLIES)
        fp->replies[fp->n_replies++] = *r;
}

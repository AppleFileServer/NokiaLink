/* The FBUS link. See link.h and docs/FBUS.md. */
#include "link.h"

#define DEFAULT_ACK_TIMEOUT_MS 500
#define DEFAULT_TRIES          3
/* A frame identical to the one before it, arriving within this time,
 * is the phone repeating itself because our acknowledgement was lost. */
#define DUPLICATE_WINDOW_MS    3000

#define SYNC_BYTE   0x55
#define SYNC_COUNT  32
#define SYNC_END    0xC1

void nl_link_init(nl_link *l, nl_port *port, nl_u8 *msg_buf, long msg_cap,
                  nl_msg_handler handler, void *handler_ctx)
{
    l->port = port;
    nl_fbus_parser_init(&l->parser, NL_FBUS_DEV_PC, NL_FBUS_DEV_PHONE);
    nl_fbus_asm_init(&l->as, msg_buf, msg_cap);
    l->handler = handler;
    l->handler_ctx = handler_ctx;
    l->ack_timeout_ms = DEFAULT_ACK_TIMEOUT_MS;
    l->tries = DEFAULT_TRIES;
    l->stats.frames_sent = 0;
    l->stats.frames_resent = 0;
    l->stats.acks_sent = 0;
    l->stats.duplicates = 0;
    l->stats.messages = 0;
    l->stats.stray_acks = 0;
    l->at_ok = 0;
    l->seq = 0;
    l->awaiting_ack = 0;
    l->ack_seen = 0;
    l->want_reply = 0;
    l->reply_seen = 0;
    l->reply_too_long = 0;
    l->reply_buf = 0;
    l->reply_cap = 0;
    l->reply_len = 0;
    l->have_last = 0;
    l->io_error = 0;
}

static nl_u32 now(nl_link *l)            { return l->port->now_ms(l->port->ctx); }
static void   nap(nl_link *l, nl_u32 ms) { l->port->sleep_ms(l->port->ctx, ms); }

static int put(nl_link *l, const nl_u8 *bytes, long n)
{
    if (l->port->write(l->port->ctx, bytes, n) != n) {
        l->io_error = 1;
        return NL_ERR_IO;
    }
    return NL_OK;
}

/* ---- Start-up ---------------------------------------------------- */

/* Send a modem command, then listen for wait_ms and count an "OK". */
static int at_command(nl_link *l, const char *cmd, nl_u32 wait_ms)
{
    nl_u8 buf[32];
    long n, i, len = 0;
    nl_u32 start;
    nl_u8 prev = 0;
    int ok = 0;

    while (cmd[len]) len++;
    if (put(l, (const nl_u8 *)cmd, len) != NL_OK)
        return NL_ERR_IO;

    start = now(l);
    while (now(l) - start < wait_ms) {
        n = l->port->read(l->port->ctx, buf, (long)sizeof buf);
        if (n < 0) { l->io_error = 1; return NL_ERR_IO; }
        if (n == 0) { nap(l, 5); continue; }
        for (i = 0; i < n; i++) {
            if (prev == 'O' && buf[i] == 'K') ok = 1;
            prev = buf[i];
        }
    }
    if (ok) l->at_ok++;
    return NL_OK;
}

int nl_link_start_dlr3(nl_link *l)
{
    nl_u8 sync[SYNC_COUNT + 1];
    nl_u8 junk[32];
    nl_u32 start;
    long n;
    int i;

    l->at_ok = 0;
    l->io_error = 0;

    /* Power the cable's converter off and on again. */
    if (l->port->set_power(l->port->ctx, 0) != 0) return NL_ERR_IO;
    nap(l, 1000);
    if (l->port->set_power(l->port->ctx, 1) != 0) return NL_ERR_IO;
    nap(l, 1000);

    /* The phone starts as a modem. Ask it to speak FBUS instead. */
    if (l->port->set_speed(l->port->ctx, 19200L) != 0) return NL_ERR_IO;
    if (at_command(l, "AT\r\n", 100) != NL_OK) return NL_ERR_IO;
    if (at_command(l, "AT&F\r\n", 100) != NL_OK) return NL_ERR_IO;
    if (at_command(l, "AT*NOKIAFBUS\r\n", 100) != NL_OK) return NL_ERR_IO;

    /* FBUS runs at 115,200. A run of 0x55 lets the phone find the bits. */
    if (l->port->set_speed(l->port->ctx, 115200L) != 0) return NL_ERR_IO;
    for (i = 0; i < SYNC_COUNT; i++) sync[i] = SYNC_BYTE;
    sync[SYNC_COUNT] = SYNC_END;
    if (put(l, sync, SYNC_COUNT + 1) != NL_OK) return NL_ERR_IO;

    /* Let the phone settle, discarding anything left from the change. */
    start = now(l);
    while (now(l) - start < 1000) {
        n = l->port->read(l->port->ctx, junk, (long)sizeof junk);
        if (n < 0) { l->io_error = 1; return NL_ERR_IO; }
        if (n == 0) nap(l, 10);
    }

    nl_fbus_parser_reset(&l->parser);
    l->as.expect = 0;
    l->have_last = 0;
    l->seq = 0;
    return NL_OK;
}

/* ---- Receiving --------------------------------------------------- */

static int send_ack(nl_link *l, nl_u8 type, nl_u8 seq)
{
    nl_u8 frame[16];
    long n = nl_fbus_encode_ack(frame, type, seq);

    l->stats.acks_sent++;
    return put(l, frame, n);
}

/* Is the frame in the parser the same one as last time, sent again? */
static int is_repeat(nl_link *l)
{
    nl_fbus_parser *p = &l->parser;
    nl_u8 seq;

    if (!l->have_last || p->length < 2) return 0;
    seq = p->block[p->length - 1];
    return l->last_type == p->type && l->last_seq == seq &&
           l->last_len == p->length &&
           l->last_sum1 == p->sum1 && l->last_sum2 == p->sum2 &&
           now(l) - l->last_time < DUPLICATE_WINDOW_MS;
}

static void remember(nl_link *l)
{
    nl_fbus_parser *p = &l->parser;

    l->have_last = 1;
    l->last_type = p->type;
    l->last_seq = p->block[p->length - 1];
    l->last_len = p->length;
    l->last_sum1 = p->sum1;
    l->last_sum2 = p->sum2;
    l->last_time = now(l);
}

static void take_frame(nl_link *l)
{
    nl_fbus_parser *p = &l->parser;
    nl_fbus_event ev;
    long i;

    if (p->type != NL_FBUS_TYPE_ACK && is_repeat(l)) {
        l->stats.duplicates++;
        l->last_time = now(l);
        if (p->type != NL_FBUS_TYPE_TRACE)
            send_ack(l, p->type, (nl_u8)(p->block[p->length - 1] & 0x0F));
        return;
    }

    nl_fbus_asm_frame(&l->as, p->type, p->block, (long)p->length, &ev);

    if (ev.flags & NL_FB_GOT_ACK) {
        if (l->awaiting_ack && ev.ack_type == l->ack_type &&
            (ev.ack_seq & 0x07) == (l->ack_seq & 0x07))
            l->ack_seen = 1;
        else
            l->stats.stray_acks++;
        return;
    }
    if (ev.flags & NL_FB_DROPPED)
        return;

    remember(l);
    if (ev.flags & NL_FB_SEND_ACK)
        send_ack(l, ev.reply_type, ev.reply_seq);

    if (ev.flags & NL_FB_MESSAGE) {
        l->stats.messages++;
        if (l->want_reply && !l->reply_seen && ev.msg_type == l->want_type) {
            l->reply_seen = 1;
            if (ev.msg_len > l->reply_cap) {
                l->reply_too_long = 1;
                l->reply_len = ev.msg_len;
            } else {
                for (i = 0; i < ev.msg_len; i++)
                    l->reply_buf[i] = ev.msg[i];
                l->reply_len = ev.msg_len;
            }
        } else if (l->handler) {
            l->handler(l->handler_ctx, ev.msg_type, ev.msg, ev.msg_len);
        }
    }
}

long nl_link_poll(nl_link *l)
{
    nl_u8 buf[64];
    long n, i, total = 0;

    for (;;) {
        n = l->port->read(l->port->ctx, buf, (long)sizeof buf);
        if (n < 0) { l->io_error = 1; return NL_ERR_IO; }
        if (n == 0) break;
        for (i = 0; i < n; i++)
            if (nl_fbus_parser_byte(&l->parser, buf[i]))
                take_frame(l);
        total += n;
        if (l->io_error) return NL_ERR_IO;
    }
    return total;
}

/* ---- Sending ----------------------------------------------------- */

static int send_frame(nl_link *l, nl_u8 type, const nl_u8 *data, long len,
                      nl_u8 to_go, nl_u8 seq)
{
    nl_u8 frame[NL_FBUS_MAX_FRAME];
    long n = nl_fbus_encode_data(frame, type, data, len, to_go, seq);
    nl_u32 start;
    long got;
    int attempt;

    l->awaiting_ack = 1;
    l->ack_type = type;
    l->ack_seq = seq;
    l->ack_seen = 0;

    for (attempt = 0; attempt < l->tries; attempt++) {
        if (put(l, frame, n) != NL_OK) { l->awaiting_ack = 0; return NL_ERR_IO; }
        l->stats.frames_sent++;
        if (attempt > 0) l->stats.frames_resent++;

        start = now(l);
        while (!l->ack_seen && now(l) - start < l->ack_timeout_ms) {
            got = nl_link_poll(l);
            if (got < 0) { l->awaiting_ack = 0; return NL_ERR_IO; }
            if (got == 0) nap(l, 1);
        }
        if (l->ack_seen) { l->awaiting_ack = 0; return NL_OK; }
    }
    l->awaiting_ack = 0;
    return NL_ERR_NO_ACK;
}

int nl_link_send(nl_link *l, nl_u8 type, const nl_u8 *data, long len)
{
    long frames = (len + NL_FBUS_CHUNK - 1) / NL_FBUS_CHUNK;
    long i, off = 0, part;
    nl_u8 seq;
    int err;

    if (frames < 1) frames = 1;
    if (frames > 255) return NL_ERR_TOO_LONG;

    for (i = 0; i < frames; i++) {
        part = len - off;
        if (part > NL_FBUS_CHUNK) part = NL_FBUS_CHUNK;
        seq = l->seq;
        if (i == 0) seq |= NL_FBUS_SEQ_FIRST;
        l->seq = (nl_u8)((l->seq + 1) & 0x07);

        err = send_frame(l, type, data + off, part, (nl_u8)(frames - i), seq);
        if (err != NL_OK) return err;
        off += part;
    }
    return NL_OK;
}

int nl_link_request(nl_link *l, nl_u8 type, const nl_u8 *data, long len,
                    nl_u8 reply_type, nl_u8 *reply, long reply_cap,
                    long *reply_len, nl_u32 timeout_ms)
{
    nl_u32 start;
    long got;
    int err;

    /* Set up the catch first: the reply can follow the acknowledgement
     * at once. */
    l->want_reply = 1;
    l->want_type = reply_type;
    l->reply_seen = 0;
    l->reply_too_long = 0;
    l->reply_buf = reply;
    l->reply_cap = reply_cap;
    l->reply_len = 0;
    if (reply_len) *reply_len = 0;

    err = nl_link_send(l, type, data, len);
    if (err != NL_OK) { l->want_reply = 0; return err; }

    start = now(l);
    while (!l->reply_seen && now(l) - start < timeout_ms) {
        got = nl_link_poll(l);
        if (got < 0) { l->want_reply = 0; return NL_ERR_IO; }
        if (got == 0) nap(l, 1);
    }
    l->want_reply = 0;

    if (!l->reply_seen) return NL_ERR_TIMEOUT;
    if (reply_len) *reply_len = l->reply_len;
    return l->reply_too_long ? NL_ERR_TOO_LONG : NL_OK;
}

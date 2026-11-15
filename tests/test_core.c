/* Host tests for the FBUS framing (src/core/fbus.c) and the link
 * (src/core/link.c), against the pretend phone. Run by tests/run-host.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/core/link.h"
#include "fakephone.h"

static int checks, failures;
static const char *current;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #cond); } } while (0)

static int same(const nl_u8 *a, const nl_u8 *b, long n) { return memcmp(a, b, (size_t)n) == 0; }

/* Frames quoted in published descriptions of FBUS. */
static const nl_u8 GET_VERSION[] = {
    0x1E, 0x00, 0x0C, 0xD1, 0x00, 0x07, 0x00, 0x01, 0x00, 0x03, 0x00,
    0x01, 0x60, 0x00, 0x72, 0xD5 };
static const nl_u8 PHONE_ACK[] = {      /* the phone acknowledging it */
    0x1E, 0x0C, 0x00, 0x7F, 0x00, 0x02, 0xD1, 0x00, 0xCF, 0x71 };
static const nl_u8 PC_ACK[] = {         /* the computer acknowledging type D2, sequence 1 */
    0x1E, 0x00, 0x0C, 0x7F, 0x00, 0x02, 0xD2, 0x01, 0xC0, 0x7C };

static const nl_u8 VERSION_REQ[] = { 0x00, 0x01, 0x00, 0x03, 0x00 };

/* ---- Framing ----------------------------------------------------- */

static void test_encode(void)
{
    nl_u8 out[NL_FBUS_MAX_FRAME];
    nl_u8 even[] = { 1, 2, 3, 4 };
    long n;

    current = "encode";
    n = nl_fbus_encode_data(out, 0xD1, VERSION_REQ, 5, 0x01, 0x60);
    CHECK(n == (long)sizeof GET_VERSION);
    CHECK(same(out, GET_VERSION, n));

    n = nl_fbus_encode_ack(out, 0xD2, 0x01);
    CHECK(n == (long)sizeof PC_ACK);
    CHECK(same(out, PC_ACK, n));

    /* An even body takes no padding: 6 + (4 + 2) + 2. */
    n = nl_fbus_encode_data(out, 0x14, even, 4, 1, 0x41);
    CHECK(n == 14);
    CHECK(out[5] == 6 && out[10] == 1 && out[11] == 0x41);

    /* The longest frame fits the buffer size the header promises. */
    {
        nl_u8 big[NL_FBUS_CHUNK];
        memset(big, 0xAA, sizeof big);
        n = nl_fbus_encode_data(out, 0x14, big, NL_FBUS_CHUNK, 1, 0x40);
        CHECK(n == NL_FBUS_MAX_FRAME);
    }
}

static int feed(nl_fbus_parser *p, const nl_u8 *b, long n)
{
    int frames = 0;
    long i;
    for (i = 0; i < n; i++) frames += nl_fbus_parser_byte(p, b[i]);
    return frames;
}

static void test_parser(void)
{
    nl_fbus_parser p;
    nl_u8 bad[sizeof PHONE_ACK];
    nl_u8 junk[] = { 0x55, 0x00, 0x1E, 0x33, 0xFF };
    nl_u8 huge[] = { 0x1E, 0x0C, 0x00, 0x14, 0x7F, 0xFF };

    current = "parser";
    nl_fbus_parser_init(&p, NL_FBUS_DEV_PC, NL_FBUS_DEV_PHONE);

    CHECK(feed(&p, PHONE_ACK, sizeof PHONE_ACK) == 1);
    CHECK(p.type == 0x7F && p.length == 2);
    CHECK(p.block[0] == 0xD1 && p.block[1] == 0x00);
    CHECK(p.sum1 == 0xCF && p.sum2 == 0x71);

    /* Rubbish, including a false frame marker, then a good frame. */
    CHECK(feed(&p, junk, sizeof junk) == 0);
    CHECK(feed(&p, PHONE_ACK, sizeof PHONE_ACK) == 1);
    CHECK(p.skipped_bytes > 0);

    /* A damaged frame is counted and thrown away; the next one is read. */
    memcpy(bad, PHONE_ACK, sizeof bad);
    bad[6] ^= 0x01;
    CHECK(feed(&p, bad, sizeof bad) == 0);
    CHECK(p.bad_checksums == 1);
    CHECK(feed(&p, PHONE_ACK, sizeof PHONE_ACK) == 1);

    /* A frame addressed the other way is not ours. */
    CHECK(feed(&p, GET_VERSION, sizeof GET_VERSION) == 0);
    CHECK(feed(&p, PHONE_ACK, sizeof PHONE_ACK) == 1);

    /* A frame marker straight after a frame marker starts again. */
    {
        nl_u8 one = 0x1E;
        CHECK(feed(&p, &one, 1) == 0);
        CHECK(feed(&p, PHONE_ACK, sizeof PHONE_ACK) == 1);
    }

    /* An impossible length is refused without overrunning anything. */
    CHECK(feed(&p, huge, sizeof huge) == 0);
    CHECK(p.too_long == 1);
    CHECK(feed(&p, PHONE_ACK, sizeof PHONE_ACK) == 1);
    CHECK(p.good_frames == 6);
}

static void test_assembler(void)
{
    nl_u8 buf[16];
    nl_fbus_asm a;
    nl_fbus_event ev;
    nl_u8 f1[] = { 'a', 'b', 'c', 3, 0x42 };
    nl_u8 f2[] = { 'd', 'e', 2, 0x03 };
    nl_u8 f3[] = { 'f', 1, 0x04 };
    nl_u8 ack[] = { 0xD1, 0x05 };
    nl_u8 trace[] = { 'x', 1, 0x40 };
    nl_u8 big1[] = { 1,2,3,4,5,6,7,8,9,10, 2, 0x40 };
    nl_u8 big2[] = { 1,2,3,4,5,6,7,8,9,10, 1, 0x01 };

    current = "assembler";
    nl_fbus_asm_init(&a, buf, sizeof buf);

    nl_fbus_asm_frame(&a, 0x14, f1, sizeof f1, &ev);
    CHECK(ev.flags == NL_FB_SEND_ACK && ev.reply_type == 0x14 && ev.reply_seq == 0x02);
    nl_fbus_asm_frame(&a, 0x14, f2, sizeof f2, &ev);
    CHECK(ev.flags == NL_FB_SEND_ACK && ev.reply_seq == 0x03);
    nl_fbus_asm_frame(&a, 0x14, f3, sizeof f3, &ev);
    CHECK(ev.flags == (NL_FB_SEND_ACK | NL_FB_MESSAGE));
    CHECK(ev.msg_type == 0x14 && ev.msg_len == 6 && same(ev.msg, (const nl_u8 *)"abcdef", 6));

    /* A continuation with nothing open, or out of step, is ignored. */
    nl_fbus_asm_frame(&a, 0x14, f2, sizeof f2, &ev);
    CHECK(ev.flags == NL_FB_DROPPED);
    nl_fbus_asm_frame(&a, 0x14, f1, sizeof f1, &ev);
    nl_fbus_asm_frame(&a, 0x14, f3, sizeof f3, &ev);     /* skipped f2 */
    CHECK(ev.flags == NL_FB_DROPPED);
    nl_fbus_asm_frame(&a, 0x15, f2, sizeof f2, &ev);     /* wrong type */
    CHECK(ev.flags == NL_FB_DROPPED);
    nl_fbus_asm_frame(&a, 0x14, f2, sizeof f2, &ev);     /* the right one still fits */
    CHECK(ev.flags == NL_FB_SEND_ACK);
    CHECK(a.dropped_frames == 3);

    nl_fbus_asm_frame(&a, 0x7F, ack, sizeof ack, &ev);
    CHECK(ev.flags == NL_FB_GOT_ACK && ev.ack_type == 0xD1 && ev.ack_seq == 0x05);

    /* The phone's debug trace is delivered but never acknowledged. */
    nl_fbus_asm_frame(&a, 0x00, trace, sizeof trace, &ev);
    CHECK(ev.flags == NL_FB_MESSAGE && ev.msg_len == 1);

    /* Too long for the buffer: still acknowledged, reported, not delivered. */
    nl_fbus_asm_frame(&a, 0x14, big1, sizeof big1, &ev);
    CHECK(ev.flags == NL_FB_SEND_ACK);
    nl_fbus_asm_frame(&a, 0x14, big2, sizeof big2, &ev);
    CHECK(ev.flags == (NL_FB_SEND_ACK | NL_FB_OVERFLOW));
    CHECK(a.overflows == 1);
}

/* ---- Link -------------------------------------------------------- */

static nl_u8 msg_buf[2048];

static struct { int count; nl_u8 type; long len; nl_u8 data[64]; } heard;

static void on_message(void *ctx, nl_u8 type, const nl_u8 *data, long len)
{
    (void)ctx;
    heard.count++;
    heard.type = type;
    heard.len = len;
    memcpy(heard.data, data, (size_t)(len > 64 ? 64 : len));
}

static void start(fakephone *fp, nl_link *l)
{
    fp_init(fp);
    nl_link_init(l, &fp->port, msg_buf, sizeof msg_buf, on_message, 0);
    memset(&heard, 0, sizeof heard);
}

static void test_startup(void)
{
    fakephone fp; nl_link l;
    nl_u8 want[64];
    long n = 0;
    nl_u32 t0;
    int i;

    current = "startup";
    start(&fp, &l);
    fp.fbus = 0;
    t0 = fp.clock;
    CHECK(nl_link_start_dlr3(&l) == NL_OK);
    CHECK(strcmp(fp.log, "P0 P1 S19200 S115200") == 0);
    CHECK(l.at_ok == 3);
    CHECK(fp.clock - t0 >= 3300 && fp.clock - t0 < 4000);

    memcpy(want + n, "AT\r\n", 4); n += 4;
    memcpy(want + n, "AT&F\r\n", 6); n += 6;
    memcpy(want + n, "AT*NOKIAFBUS\r\n", 14); n += 14;
    for (i = 0; i < 32; i++) want[n++] = 0x55;
    want[n++] = 0xC1;
    CHECK(fp.written_len == n);
    CHECK(same(fp.written, want, n));

    /* A phone that says nothing still gets the whole sequence. */
    start(&fp, &l);
    fp.fbus = 0;
    fp.answer_at = 0;
    CHECK(nl_link_start_dlr3(&l) == NL_OK);
    CHECK(l.at_ok == 0);
    CHECK(fp.written_len == n);
}

static const nl_u8 VERSION_REPLY[] = {
    0x01, 0x00, 0x00, 0x03, 'V', ' ', '0', '4', '.', '8', '0', '\n',
    '2', '3', '-', '0', '2', '-', '0', '0', '\n', 'N', 'S', 'E', '-', '5' };

static void test_request(void)
{
    fakephone fp; nl_link l;
    fp_reply r;
    nl_u8 reply[64];
    long len = -1;

    current = "request";
    start(&fp, &l);
    memset(&r, 0, sizeof r);
    r.req_type = 0xD1; r.reply_type = 0xD2;
    r.data = VERSION_REPLY; r.len = sizeof VERSION_REPLY; r.chunk = 100;
    fp_add_reply(&fp, &r);

    CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, sizeof reply, &len, 1000) == NL_OK);
    CHECK(len == (long)sizeof VERSION_REPLY && same(reply, VERSION_REPLY, len));
    /* The very first frame on the wire is the published one but for the
     * sequence byte, which starts at 0 with the first-frame mark. */
    CHECK(fp.frames_seen == 1 && fp.seen_seq[0] == 0x40 && fp.seen_to_go[0] == 1);
    CHECK(fp.msg_len == 5 && same(fp.msg, VERSION_REQ, 5));
    CHECK(fp.acks_seen == 1 && fp.ack_type[0] == 0xD2 && fp.ack_seq[0] == 0x00);
    CHECK(l.stats.frames_sent == 1 && l.stats.frames_resent == 0);
    CHECK(l.stats.messages == 1 && heard.count == 0);
}

static void test_retry(void)
{
    fakephone fp; nl_link l;
    nl_u32 t0;

    current = "retry";
    start(&fp, &l);
    fp.ignore_frames = 1;
    CHECK(nl_link_send(&l, 0xD1, VERSION_REQ, 5) == NL_OK);
    CHECK(fp.frames_seen == 2);
    CHECK(fp.seen_seq[0] == fp.seen_seq[1]);       /* the same frame again */
    CHECK(l.stats.frames_sent == 2 && l.stats.frames_resent == 1);

    /* A phone that never answers: three tries, then give up. */
    start(&fp, &l);
    fp.ignore_frames = 99;
    t0 = fp.clock;
    CHECK(nl_link_send(&l, 0xD1, VERSION_REQ, 5) == NL_ERR_NO_ACK);
    CHECK(fp.frames_seen == 3);
    CHECK(fp.clock - t0 >= 1500 && fp.clock - t0 < 1600);

    /* Acknowledged but never answered. */
    start(&fp, &l);
    fp.never_reply = 1;
    {
        nl_u8 reply[8]; long len;
        t0 = fp.clock;
        CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, sizeof reply, &len, 700) == NL_ERR_TIMEOUT);
        CHECK(fp.clock - t0 >= 700 && fp.clock - t0 < 800);
    }
}

static void test_noise(void)
{
    fakephone fp; nl_link l;
    fp_reply r;
    nl_u8 reply[64];
    long len;

    /* The phone repeats each frame (it missed our acknowledgement). */
    current = "repeated frames";
    start(&fp, &l);
    memset(&r, 0, sizeof r);
    r.req_type = 0xD1; r.reply_type = 0xD2;
    r.data = VERSION_REPLY; r.len = sizeof VERSION_REPLY; r.chunk = 10;
    r.repeat_frames = 1;
    fp_add_reply(&fp, &r);
    CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, sizeof reply, &len, 1000) == NL_OK);
    CHECK(len == (long)sizeof VERSION_REPLY && same(reply, VERSION_REPLY, len));
    CHECK(l.stats.duplicates == 3);                /* 26 bytes in tens: 3 frames */
    CHECK(fp.acks_seen == 6);                      /* each copy acknowledged */
    CHECK(l.stats.messages == 1);

    /* A damaged copy arrives before the good one. */
    current = "damaged frame";
    start(&fp, &l);
    r.repeat_frames = 0; r.corrupt_first = 1; r.chunk = 100;
    fp_add_reply(&fp, &r);
    CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, sizeof reply, &len, 1000) == NL_OK);
    CHECK(same(reply, VERSION_REPLY, len));
    CHECK(l.parser.bad_checksums == 1);
    CHECK(fp.acks_seen == 1);

    /* The caller's buffer is too small: said so, nothing overrun. */
    current = "small buffer";
    start(&fp, &l);
    r.corrupt_first = 0;
    fp_add_reply(&fp, &r);
    reply[8] = 0xEE;
    CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, 8, &len, 1000) == NL_ERR_TOO_LONG);
    CHECK(len == (long)sizeof VERSION_REPLY && reply[8] == 0xEE);
}

static void test_long(void)
{
    fakephone fp; nl_link l;
    fp_reply r;
    static nl_u8 out[250], back[300], reply[512];
    long len, i;

    current = "long messages";
    for (i = 0; i < 250; i++) out[i] = (nl_u8)(i * 7 + 1);
    for (i = 0; i < 300; i++) back[i] = (nl_u8)(255 - i);

    start(&fp, &l);
    memset(&r, 0, sizeof r);
    r.req_type = 0x13; r.reply_type = 0x13;
    r.data = back; r.len = 300; r.chunk = 100;
    fp_add_reply(&fp, &r);
    CHECK(nl_link_request(&l, 0x13, out, 250, 0x13, reply, sizeof reply, &len, 1000) == NL_OK);

    /* Out: 120 + 120 + 10 bytes, counting down, first frame marked. */
    CHECK(fp.frames_seen == 3);
    CHECK(fp.seen_len[0] == 120 && fp.seen_len[1] == 120 && fp.seen_len[2] == 10);
    CHECK(fp.seen_to_go[0] == 3 && fp.seen_to_go[1] == 2 && fp.seen_to_go[2] == 1);
    CHECK(fp.seen_seq[0] == 0x40 && fp.seen_seq[1] == 0x01 && fp.seen_seq[2] == 0x02);
    CHECK(fp.msg_len == 250 && same(fp.msg, out, 250));

    /* Back: three frames joined. */
    CHECK(len == 300 && same(reply, back, 300));
    CHECK(fp.acks_seen == 3);

    /* An empty message is still one frame. */
    start(&fp, &l);
    CHECK(nl_link_send(&l, 0x64, 0, 0) == NL_OK);
    CHECK(fp.frames_seen == 1 && fp.seen_len[0] == 0 && fp.seen_to_go[0] == 1);
}

static void test_unasked(void)
{
    fakephone fp; nl_link l;
    fp_reply r;
    nl_u8 call[] = { 0x01, 0x08, 0x00, 0x05, 0x01 };
    nl_u8 reply[64];
    long len;
    int i;

    current = "unasked messages";
    start(&fp, &l);
    fp_send_message(&fp, 0x01, call, sizeof call, 100, 0, 0);
    CHECK(nl_link_poll(&l) > 0);
    CHECK(heard.count == 1 && heard.type == 0x01 && heard.len == 5 && same(heard.data, call, 5));
    CHECK(fp.acks_seen == 1 && fp.ack_type[0] == 0x01);
    CHECK(nl_link_poll(&l) == 0);

    /* Something else arrives while a reply is awaited: both are kept apart. */
    start(&fp, &l);
    memset(&r, 0, sizeof r);
    r.req_type = 0xD1; r.reply_type = 0xD2;
    r.data = VERSION_REPLY; r.len = sizeof VERSION_REPLY; r.chunk = 100;
    r.extra_before = 1;
    fp_add_reply(&fp, &r);
    CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, sizeof reply, &len, 1000) == NL_OK);
    CHECK(same(reply, VERSION_REPLY, len));
    CHECK(heard.count == 1 && heard.type == 0x0A);

    /* Sequence numbers run 0 to 7 and round again; every request still works. */
    current = "sequence wrap";
    start(&fp, &l);
    r.extra_before = 0;
    fp_add_reply(&fp, &r);
    for (i = 0; i < 20; i++)
        CHECK(nl_link_request(&l, 0xD1, VERSION_REQ, 5, 0xD2, reply, sizeof reply, &len, 1000) == NL_OK);
    CHECK(fp.frames_seen == 20);
    CHECK(fp.seen_seq[7] == 0x47 && fp.seen_seq[8] == 0x40 && fp.seen_seq[19] == 0x43);
    CHECK(l.stats.duplicates == 0 && l.stats.stray_acks == 0);
}

int main(void)
{
    test_encode();
    test_parser();
    test_assembler();
    test_startup();
    test_request();
    test_retry();
    test_noise();
    test_long();
    test_unasked();

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

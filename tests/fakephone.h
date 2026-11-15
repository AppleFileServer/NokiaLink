/* A pretend phone for the host tests: an nl_port whose other end is a
 * scripted FBUS device with its own clock. Nothing here is built into
 * NokiaLink itself. */
#ifndef FAKEPHONE_H
#define FAKEPHONE_H

#include "../src/core/link.h"

#define FP_QUEUE   16384
#define FP_REPLIES 4
#define FP_FRAMES  64

typedef struct {
    nl_u8 req_type;        /* when a whole message of this type arrives... */
    nl_u8 reply_type;      /* ...answer with a message of this type */
    const nl_u8 *data;
    long len;
    long chunk;            /* message bytes per frame */
    int repeat_frames;     /* send every frame twice */
    int corrupt_first;     /* send a damaged copy of the first frame first */
    int extra_before;      /* send an unrelated message (type 0x0A) first */
} fp_reply;

typedef struct {
    nl_port port;
    nl_u32 clock;                      /* milliseconds */

    nl_u8 to_pc[FP_QUEUE];             /* bytes waiting to be read */
    long head, tail;

    nl_u8 written[FP_QUEUE];           /* everything the computer sent */
    long written_len;
    char log[128];                     /* "P0 P1 S19200 S115200" */

    int fbus;                          /* 0 = modem commands, 1 = FBUS */
    int answer_at;                     /* say OK to modem commands */
    int ignore_frames;                 /* don't acknowledge the next N frames */
    int never_reply;                   /* acknowledge, but send no replies */
    fp_reply replies[FP_REPLIES];
    int n_replies;

    nl_fbus_parser parser;
    nl_u8 seq;                         /* the phone's own sequence counter */

    /* What the phone saw. */
    int frames_seen;
    nl_u8 seen_to_go[FP_FRAMES], seen_seq[FP_FRAMES];
    long seen_len[FP_FRAMES];
    nl_u8 msg[4096];                   /* message being put together */
    long msg_len;
    int messages_seen;
    int acks_seen;
    nl_u8 ack_type[FP_FRAMES], ack_seq[FP_FRAMES];
} fakephone;

void fp_init(fakephone *fp);
void fp_add_reply(fakephone *fp, const fp_reply *r);
/* Queue a message from the phone, split into frames of chunk bytes. */
void fp_send_message(fakephone *fp, nl_u8 type, const nl_u8 *data, long len,
                     long chunk, int repeat_frames, int corrupt_first);

#endif

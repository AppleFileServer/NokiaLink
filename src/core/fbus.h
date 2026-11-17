/* FBUS version 2 framing for NokiaLink.
 *
 * Portable C with no Macintosh or POSIX calls and no memory allocation:
 * the same source is built for the Plus and for the host tools.
 * Written from the protocol description (docs/FBUS.md); no Gammu code.
 *
 * Three pieces, none of which does any input or output:
 *   - encoding a frame into bytes,
 *   - a parser that is fed received bytes one at a time,
 *   - an assembler that joins the frames of a long message.
 */
#ifndef NL_FBUS_H
#define NL_FBUS_H

#ifndef NL_BASIC_TYPES
#define NL_BASIC_TYPES
typedef unsigned char  nl_u8;
typedef unsigned short nl_u16;
typedef unsigned long  nl_u32;
#endif

#define NL_FBUS_FRAME_ID   0x1E   /* first byte of every frame on a cable */
#define NL_FBUS_DEV_PHONE  0x00
#define NL_FBUS_DEV_PC     0x0C   /* the computer: here, the Plus */
#define NL_FBUS_TYPE_ACK   0x7F   /* frame type of an acknowledgement */
#define NL_FBUS_TYPE_TRACE 0x00   /* the phone's debug trace: never acknowledged */
#define NL_FBUS_SEQ_FIRST  0x40   /* set in the sequence byte of a message's first frame */

#define NL_FBUS_CHUNK      120    /* most message bytes carried by one frame we send */
#define NL_FBUS_MAX_BLOCK  512    /* longest frame body accepted from the phone */

/* Room for any frame nl_fbus_encode_data can produce:
 * 6 header + 120 data + 2 trailer + 2 checksums (even already). */
#define NL_FBUS_MAX_FRAME  (6 + NL_FBUS_CHUNK + 2 + 2)

/* ---- Encoding ---------------------------------------------------- */

/* The general form: a frame of the given type whose body is block[0..len).
 * Pads an odd body with one zero byte and appends the two checksums.
 * Returns the number of bytes written to out (6 + len + pad + 2). */
long nl_fbus_encode_raw(nl_u8 *out, nl_u8 dest, nl_u8 src, nl_u8 type,
                        const nl_u8 *block, long len);

/* One frame of a message from the computer to the phone:
 * data[0..len), then frames_to_go and the sequence byte. len <= NL_FBUS_CHUNK. */
long nl_fbus_encode_data(nl_u8 *out, nl_u8 type, const nl_u8 *data, long len,
                         nl_u8 frames_to_go, nl_u8 seq);

/* The acknowledgement of a frame of the given type and sequence number. */
long nl_fbus_encode_ack(nl_u8 *out, nl_u8 type, nl_u8 seq);

/* ---- Parser ------------------------------------------------------ */

typedef struct {
    /* Set by nl_fbus_parser_init. */
    nl_u8  want_dest, want_src;

    /* The last good frame (valid when nl_fbus_parser_byte returns 1). */
    nl_u8  type;
    nl_u16 length;                 /* body length, without padding */
    nl_u8  block[NL_FBUS_MAX_BLOCK + 1 + 2]; /* body, padding, checksums */
    nl_u8  sum1, sum2;             /* the frame's two checksum bytes */

    /* Counters, for the link's reliability figures. */
    nl_u32 good_frames;
    nl_u32 bad_checksums;
    nl_u32 too_long;
    nl_u32 skipped_bytes;          /* bytes thrown away while looking for a frame */

    /* Private. */
    nl_u8  state;
    nl_u16 pos;                    /* offset of the next byte within its frame */
    nl_u16 count, need;
    nl_u8  xor_even, xor_odd;
} nl_fbus_parser;

/* want_dest and want_src are the addresses a frame must carry to be
 * accepted: NL_FBUS_DEV_PC and NL_FBUS_DEV_PHONE to listen to the phone. */
void nl_fbus_parser_init(nl_fbus_parser *p, nl_u8 want_dest, nl_u8 want_src);

/* Forget any half-received frame (after a speed change, say). */
void nl_fbus_parser_reset(nl_fbus_parser *p);

/* Feed one received byte. Returns 1 when it completes a frame with good
 * checksums (read p->type, p->block, p->length), otherwise 0. */
int nl_fbus_parser_byte(nl_fbus_parser *p, nl_u8 b);

/* ---- Assembler --------------------------------------------------- */

#define NL_FB_GOT_ACK    0x01   /* the frame was an acknowledgement */
#define NL_FB_SEND_ACK   0x02   /* acknowledge this frame */
#define NL_FB_MESSAGE    0x04   /* a whole message is ready */
#define NL_FB_OVERFLOW   0x08   /* a message ended but was too long to keep */
#define NL_FB_DROPPED    0x10   /* the frame made no sense and was ignored */

typedef struct {
    int    flags;
    nl_u8  ack_type, ack_seq;      /* NL_FB_GOT_ACK: what the phone acknowledged */
    nl_u8  reply_type, reply_seq;  /* NL_FB_SEND_ACK: what to acknowledge */
    nl_u8  msg_type;               /* NL_FB_MESSAGE */
    const nl_u8 *msg;
    long   msg_len;
} nl_fbus_event;

typedef struct {
    nl_u8 *buf;                    /* the caller's buffer for whole messages */
    long   cap;
    long   len;
    nl_u8  type;
    nl_u8  expect;                 /* frames still to come; 0 = no message open */
    nl_u8  overflow;
    nl_u32 dropped_frames;
    nl_u32 overflows;
} nl_fbus_asm;

void nl_fbus_asm_init(nl_fbus_asm *a, nl_u8 *buf, long cap);

/* Take one good frame from the parser and say what follows from it. */
void nl_fbus_asm_frame(nl_fbus_asm *a, nl_u8 type, const nl_u8 *block,
                       long len, nl_fbus_event *ev);

#endif

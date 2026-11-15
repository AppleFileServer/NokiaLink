/* The FBUS link: start-up, sending with acknowledgement and retries,
 * receiving, and request-and-reply.
 *
 * Portable C. All input, output and timing goes through an nl_port, so
 * the same code runs over the Plus's serial chip, a POSIX serial
 * device, or the tests' pretend phone.
 */
#ifndef NL_LINK_H
#define NL_LINK_H

#include "fbus.h"

/* ---- The port ---------------------------------------------------- */

typedef struct nl_port {
    void *ctx;
    /* Send n bytes. Returns n, or a negative number on failure. */
    long (*write)(void *ctx, const nl_u8 *bytes, long n);
    /* Take up to max bytes that have already arrived, without waiting.
     * Returns how many (0 = none yet), or a negative number on failure. */
    long (*read)(void *ctx, nl_u8 *bytes, long max);
    /* Set the speed in baud (19200 or 115200 are asked for). 0 = done. */
    int  (*set_speed)(void *ctx, long baud);
    /* Raise (1) or drop (0) the control lines that power the cable. */
    int  (*set_power)(void *ctx, int on);
    /* A clock in milliseconds; only differences are used. */
    nl_u32 (*now_ms)(void *ctx);
    /* Wait about ms milliseconds. */
    void (*sleep_ms)(void *ctx, nl_u32 ms);
} nl_port;

/* ---- Results ----------------------------------------------------- */

#define NL_OK            0
#define NL_ERR_IO       (-1)   /* the port failed */
#define NL_ERR_NO_ACK   (-2)   /* the phone never acknowledged a frame */
#define NL_ERR_TIMEOUT  (-3)   /* acknowledged, but no reply came */
#define NL_ERR_TOO_LONG (-4)   /* the reply didn't fit the caller's buffer */

/* ---- The link ---------------------------------------------------- */

/* Called for each whole message from the phone that nobody was waiting
 * for (an incoming call, a new text). data is valid only during the
 * call. The handler must not send on the link itself. */
typedef void (*nl_msg_handler)(void *ctx, nl_u8 type, const nl_u8 *data, long len);

typedef struct {
    nl_u32 frames_sent;
    nl_u32 frames_resent;
    nl_u32 acks_sent;
    nl_u32 duplicates;      /* frames the phone repeated; acknowledged again, not delivered twice */
    nl_u32 messages;        /* whole messages received */
    nl_u32 stray_acks;      /* acknowledgements nobody was waiting for */
} nl_link_stats;

typedef struct {
    nl_port *port;
    nl_fbus_parser parser;
    nl_fbus_asm as;
    nl_msg_handler handler;
    void *handler_ctx;

    /* Settings; nl_link_init gives the defaults. */
    nl_u32 ack_timeout_ms;  /* how long to wait for an acknowledgement */
    int    tries;           /* times a frame is sent before giving up */

    nl_link_stats stats;
    int at_ok;              /* "OK" replies seen during the last start-up */

    /* Private. */
    nl_u8  seq;
    nl_u8  awaiting_ack, ack_type, ack_seq, ack_seen;
    nl_u8  want_reply, want_type, reply_seen, reply_too_long;
    nl_u8 *reply_buf;
    long   reply_cap, reply_len;
    nl_u8  have_last, last_type, last_seq, last_sum1, last_sum2;
    nl_u16 last_len;
    nl_u32 last_time;
    int    io_error;
} nl_link;

/* msg_buf (of msg_cap bytes) holds the message being received: it sets
 * the longest message the link can take. handler may be 0. */
void nl_link_init(nl_link *l, nl_port *port, nl_u8 *msg_buf, long msg_cap,
                  nl_msg_handler handler, void *handler_ctx);

/* Start a session over a DLR-3 cable: power the cable, switch the phone
 * from modem commands to FBUS, change speed, synchronise. Takes about
 * three and a half seconds. NL_OK means only that the bytes went out;
 * l->at_ok says how many of the three commands the phone answered "OK". */
int nl_link_start_dlr3(nl_link *l);

/* Take in whatever has arrived: acknowledge frames, deliver messages.
 * Call it often while idle. Returns the number of bytes handled, or
 * NL_ERR_IO. */
long nl_link_poll(nl_link *l);

/* Send one message and wait for each of its frames to be acknowledged. */
int nl_link_send(nl_link *l, nl_u8 type, const nl_u8 *data, long len);

/* Send a message, then wait up to timeout_ms for a message of
 * reply_type and copy it to reply (at most reply_cap bytes; its length
 * goes to *reply_len). Other messages arriving meanwhile go to the
 * handler. */
int nl_link_request(nl_link *l, nl_u8 type, const nl_u8 *data, long len,
                    nl_u8 reply_type, nl_u8 *reply, long reply_cap,
                    long *reply_len, nl_u32 timeout_ms);

#endif

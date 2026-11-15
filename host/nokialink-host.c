/* nokialink-host: the portable core of NokiaLink, run from the modern
 * Mac against the real phone through a USB-to-serial adapter. It proves
 * the protocol code before the Plus is involved (docs/PLAN.md, Phase 1).
 *
 * Usage: nokialink-host identify <device> [trace-file]
 *
 * NOT YET TRIED AGAINST A PHONE (15 November 2026): the cable had not
 * arrived when this was written.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../src/core/link.h"
#include "port_posix.h"

static nl_u8 msg_buf[4096];

static void hex(const char *label, const nl_u8 *b, long n)
{
    long i;
    printf("%s (%ld bytes):", label, n);
    for (i = 0; i < n; i++) printf(" %02X", b[i]);
    printf("\n");
}

static void on_message(void *ctx, nl_u8 type, const nl_u8 *data, long len)
{
    char label[40];
    (void)ctx;
    snprintf(label, sizeof label, "unasked message, type %02X", type);
    hex(label, data, len);
}

static const char *error_text(int err)
{
    switch (err) {
    case NL_OK:           return "done";
    case NL_ERR_IO:       return "the serial device failed";
    case NL_ERR_NO_ACK:   return "the phone did not acknowledge";
    case NL_ERR_TIMEOUT:  return "the phone acknowledged but sent no reply";
    case NL_ERR_TOO_LONG: return "the reply was too long";
    default:              return "unknown error";
    }
}

static void print_stats(const nl_link *l)
{
    printf("frames sent %lu (repeated %lu), acknowledgements sent %lu, "
           "messages in %lu, repeats from phone %lu, bad checksums %lu, "
           "bytes skipped %lu\n",
           (unsigned long)l->stats.frames_sent, (unsigned long)l->stats.frames_resent,
           (unsigned long)l->stats.acks_sent, (unsigned long)l->stats.messages,
           (unsigned long)l->stats.duplicates, (unsigned long)l->parser.bad_checksums,
           (unsigned long)l->parser.skipped_bytes);
}

/* Ask the phone for its model and firmware version. */
static int identify(nl_link *l)
{
    static const nl_u8 req[] = { 0x00, 0x01, 0x00, 0x03, 0x00 };
    nl_u8 reply[256];
    long len = 0, i;
    int err;

    err = nl_link_start_dlr3(l);
    if (err != NL_OK) { printf("start-up: %s\n", error_text(err)); return 1; }
    printf("start-up sent; the phone answered OK to %d of 3 modem commands\n", l->at_ok);

    err = nl_link_request(l, 0xD1, req, (long)sizeof req, 0xD2,
                          reply, (long)sizeof reply, &len, 2000);
    if (err != NL_OK) { printf("identify: %s\n", error_text(err)); print_stats(l); return 1; }

    hex("reply", reply, len);
    printf("as text:\n");
    for (i = 4; i < len; i++) {
        nl_u8 c = reply[i];
        if (c == '\n') putchar('\n');
        else if (c >= 0x20 && c < 0x7F) putchar(c);
    }
    putchar('\n');
    print_stats(l);
    return 0;
}

int main(int argc, char **argv)
{
    posix_port pp;
    nl_link link;
    int result;

    if (argc < 3 || strcmp(argv[1], "identify") != 0) {
        fprintf(stderr, "usage: nokialink-host identify <device> [trace-file]\n");
        return 2;
    }
    if (posix_port_open(&pp, argv[2]) != 0) {
        fprintf(stderr, "can't open %s: %s\n", argv[2], strerror(errno));
        return 1;
    }
    if (argc > 3) {
        pp.trace = fopen(argv[3], "w");
        if (!pp.trace) {
            fprintf(stderr, "can't write %s: %s\n", argv[3], strerror(errno));
            posix_port_close(&pp);
            return 1;
        }
    }

    nl_link_init(&link, &pp.port, msg_buf, (long)sizeof msg_buf, on_message, 0);
    result = identify(&link);

    posix_port_close(&pp);
    if (pp.trace) fclose(pp.trace);
    return result;
}

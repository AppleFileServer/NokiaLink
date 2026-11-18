/* nokialink-host: the portable core of NokiaLink, run from the modern
 * Mac against the real phone through a USB-to-serial adapter. It proves
 * the protocol code before the Plus is involved (docs/PLAN.md, Phase 1).
 *
 * Usage: nokialink-host <command> <device> [trace-file]
 *   commands: identify  battery  signal  network  clock  status
 * All read-only: nothing is written to the phone.
 *
 * First worked against a real 7110 (NSE-5, firmware 04.80) on 18 Nov 2026
 * over a DLR-3P cable.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../src/core/link.h"
#include "../src/core/nlinfo.h"
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

/* One request/reply exchange; prints the error and dumps the raw reply on
 * a decode miss so an unexpected layout can be read from the trace. */
static int ask(nl_link *l, const char *what, nl_u8 type,
               const nl_u8 *req, long rn, nl_u8 reply_type,
               nl_u8 *reply, long cap, long *len)
{
    int err = nl_link_request(l, type, req, rn, reply_type, reply, cap, len, 2000);
    if (err != NL_OK) { printf("%s: %s\n", what, error_text(err)); return err; }
    return NL_OK;
}

static int do_identify(nl_link *l)
{
    static const nl_u8 req[] = { 0x00, 0x01, 0x00, 0x03, 0x00 };
    nl_u8 reply[256]; long len = 0; nl_info_ident id;

    if (ask(l, "identify", 0xD1, req, (long)sizeof req, 0xD2, reply, sizeof reply, &len) != NL_OK)
        return 1;
    if (nl_info_parse_identify(reply, len, &id) != NL_INFO_OK) {
        hex("identify reply (undecoded)", reply, len); return 1;
    }
    printf("model %s, firmware %s (%s)\n", id.model, id.version, id.date);
    return 0;
}

static int do_battery(nl_link *l)
{
    nl_u8 req[16], reply[128]; long rn, len; int pct;

    rn = nl_info_req_battery(req, sizeof req);
    if (ask(l, "battery", NL_INFO_TYPE_BATTERY, req, rn, NL_INFO_TYPE_BATTERY,
            reply, sizeof reply, &len) != NL_OK) return 1;
    if (nl_info_parse_battery(reply, len, &pct) != NL_INFO_OK) {
        hex("battery reply (undecoded)", reply, len); return 1;
    }
    printf("battery %d%%\n", pct);
    return 0;
}

static int do_signal(nl_link *l)
{
    nl_u8 req[16], reply[128]; long rn, len; int pct, err;

    rn = nl_info_req_signal(req, sizeof req);
    err = nl_link_request(l, NL_INFO_TYPE_SIGNAL, req, rn, NL_INFO_TYPE_SIGNAL,
                          reply, sizeof reply, &len, 2500);
    if (err == NL_ERR_TIMEOUT) {
        /* The 7110 (firmware 04.80) acknowledges the signal-quality request
         * but never answers it. The request is byte-for-byte what Gammu's
         * own 7110 code sends, so this is a firmware limit, not our bug.
         * Not treated as a failure. */
        printf("signal: not reported by this phone (acknowledged, no reply "
               "- a known 7110 firmware limit)\n");
        return 0;
    }
    if (err != NL_OK) { printf("signal: %s\n", error_text(err)); return 1; }
    if (nl_info_parse_signal(reply, len, &pct) != NL_INFO_OK) {
        hex("signal reply (undecoded)", reply, len); return 1;
    }
    printf("signal %d%%\n", pct);
    return 0;
}

static int do_network(nl_link *l)
{
    nl_u8 req[16], reply[256]; long rn, len; nl_info_network net;

    rn = nl_info_req_network(req, sizeof req);
    if (ask(l, "network", NL_INFO_TYPE_NETWORK, req, rn, NL_INFO_TYPE_NETWORK,
            reply, sizeof reply, &len) != NL_OK) return 1;
    if (nl_info_parse_network(reply, len, &net) != NL_INFO_OK) {
        hex("network reply (undecoded)", reply, len); return 1;
    }
    if (!net.registered) { printf("network: not registered\n"); return 0; }
    printf("network %s  (MCC/MNC %s, LAC %s, cell %s)\n",
           net.name, net.code, net.lac, net.cid);
    return 0;
}

static int do_clock(nl_link *l)
{
    nl_u8 req[16], reply[128]; long rn, len; nl_stamp clk; int err;

    rn = nl_info_req_clock(req, sizeof req);
    if (ask(l, "clock", NL_INFO_TYPE_CLOCK, req, rn, NL_INFO_TYPE_CLOCK,
            reply, sizeof reply, &len) != NL_OK) return 1;
    err = nl_info_parse_clock(reply, len, &clk);
    if (err == NL_INFO_ERR_NODATA) { printf("clock: not set on the phone\n"); return 0; }
    if (err != NL_INFO_OK) { hex("clock reply (undecoded)", reply, len); return 1; }
    printf("clock %04d-%02d-%02d %02ld:%02ld:%02ld\n",
           nl_date_year(clk.date), nl_date_month(clk.date), nl_date_day(clk.date),
           clk.secs / 3600, (clk.secs / 60) % 60, clk.secs % 60);
    return 0;
}

int main(int argc, char **argv)
{
    posix_port pp;
    nl_link link;
    const char *cmd;
    int err, bad = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: nokialink-host <command> <device> [trace-file]\n"
                        "  commands: identify battery signal network clock status\n");
        return 2;
    }
    cmd = argv[1];
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

    err = nl_link_start_dlr3(&link);
    if (err != NL_OK) {
        printf("start-up: %s\n", error_text(err));
        posix_port_close(&pp);
        if (pp.trace) fclose(pp.trace);
        return 1;
    }
    printf("start-up sent; the phone answered OK to %d of 3 modem commands\n", link.at_ok);

    if      (strcmp(cmd, "identify") == 0) bad = do_identify(&link);
    else if (strcmp(cmd, "battery")  == 0) bad = do_battery(&link);
    else if (strcmp(cmd, "signal")   == 0) bad = do_signal(&link);
    else if (strcmp(cmd, "network")  == 0) bad = do_network(&link);
    else if (strcmp(cmd, "clock")    == 0) bad = do_clock(&link);
    else if (strcmp(cmd, "status")   == 0) {
        bad |= do_identify(&link);
        bad |= do_battery(&link);
        bad |= do_signal(&link);
        bad |= do_network(&link);
        bad |= do_clock(&link);
    } else {
        fprintf(stderr, "unknown command: %s\n", cmd);
        bad = 2;
    }

    print_stats(&link);
    posix_port_close(&pp);
    if (pp.trace) fclose(pp.trace);
    return bad;
}

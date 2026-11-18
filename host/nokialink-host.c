/* nokialink-host: the portable core of NokiaLink, run from the modern
 * Mac against the real phone through a USB-to-serial adapter. It proves
 * the protocol code before the Plus is involved (docs/PLAN.md, Phase 1).
 *
 * Usage: nokialink-host <command> <device> [trace-file]
 *   commands: identify battery signal network clock contacts sms calendar calls status
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
#include "../src/core/nlpbk.h"
#include "../src/core/nlsms.h"
#include "../src/core/nlcal.h"
#include "../src/store/nlmsgarc.h"
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

static void print_entry(const nl_pbk_entry *e)
{
    int i;
    printf("  [%u] %s\n", (unsigned)e->location, e->name[0] ? e->name : "(no name)");
    for (i = 0; i < e->nnumbers; i++)
        printf("       %-7s %s\n", nl_num_type_name(e->numbers[i].type), e->numbers[i].text);
    if (e->email[0])  printf("       email   %s\n", e->email);
    if (e->postal[0]) printf("       postal  %s\n", e->postal);
    if (e->note[0])   printf("       note    %s\n", e->note);
}

/* Read one memory (ME or SIM): the count, then each entry in turn. */
static int read_memory(nl_link *l, nl_u8 mem)
{
    nl_u8 req[32], reply[1024]; long rn, len, loc;
    nl_u16 used = 0, total = 0;
    int err, found = 0;
    const char *name = nl_pbk_mem_name(mem);

    rn = nl_pbk_req_status(req, sizeof req, mem);
    err = nl_link_request(l, NL_PBK_MSG_TYPE, req, rn, NL_PBK_MSG_TYPE,
                          reply, sizeof reply, &len, 2500);
    if (err == NL_ERR_TIMEOUT)   /* first request after start-up is sometimes lost */
        err = nl_link_request(l, NL_PBK_MSG_TYPE, req, rn, NL_PBK_MSG_TYPE,
                              reply, sizeof reply, &len, 2500);
    if (err != NL_OK) { printf("%s: %s\n", name, error_text(err)); return 1; }
    if (nl_pbk_parse_status(reply, len, &used, &total) != NL_PBK_OK) {
        hex("status reply (undecoded)", reply, len); return 1;
    }
    printf("%s: %u used of %u\n", name, (unsigned)used, (unsigned)total);
    if (total == 0 || total > 2000) total = 2000;   /* guard a wild count */

    for (loc = 1; loc <= (long)total && found < (int)used; loc++) {
        nl_pbk_entry e;
        rn = nl_pbk_req_read(req, sizeof req, mem, (nl_u16)loc);
        if (rn < 0) continue;
        err = nl_link_request(l, NL_PBK_MSG_TYPE, req, rn, NL_PBK_MSG_TYPE,
                              reply, sizeof reply, &len, 3000);
        if (err != NL_OK) { printf("  [%ld] %s\n", loc, error_text(err)); continue; }
        err = nl_pbk_parse_entry(reply, len, (nl_u16)loc, &e);
        if (err == NL_PBK_ERR_EMPTY) continue;
        if (err != NL_PBK_OK) { hex("  entry reply (undecoded)", reply, len); continue; }
        print_entry(&e);
        found++;
    }
    printf("read %d of %u from %s\n", found, (unsigned)used, name);
    return 0;
}

/* Read both the phone's own memory and the SIM. */
static int do_contacts(nl_link *l)
{
    int bad = 0;
    bad |= read_memory(l, NL_PBK_MEM_ME);
    bad |= read_memory(l, NL_PBK_MEM_SIM);
    return bad;
}

#define SMS_READ_OK 0x08   /* reply sub for a message; empty slots differ */

/* A whole message after joining its parts, with a buffer for the text. */
typedef struct {
    nl_sms_pdu pdu;
    char       number[NL_SMS_NUM_MAX];
    int        has_time;
    nl_stamp   time;
    int        parts;
    char       text[4096];
} joined_msg;

/* Join the concatenated messages in msgs[0..n-1] (same sender and
 * reference, in part order) into whole messages. Returns the count. */
static int join_msgs(const nl_sms *msgs, int n, joined_msg *out, int max)
{
    static char used[512];
    int i, nout = 0;
    for (i = 0; i < n && i < 512; i++) used[i] = 0;
    for (i = 0; i < n && nout < max; i++) {
        joined_msg *j = &out[nout];
        if (used[i]) continue;
        if (!msgs[i].concat) {
            j->pdu = msgs[i].pdu;
            strncpy(j->number, msgs[i].number, sizeof j->number - 1);
            j->number[sizeof j->number - 1] = 0;
            j->has_time = msgs[i].has_time; j->time = msgs[i].time; j->parts = 1;
            strncpy(j->text, msgs[i].text, sizeof j->text - 1);
            j->text[sizeof j->text - 1] = 0;
            used[i] = 1; nout++;
            continue;
        }
        {
            int ref = msgs[i].concat_ref, total = msgs[i].concat_total;
            int got = 0, hdr = i, seq, k; long tl = 0;
            j->text[0] = 0;
            for (seq = 1; seq <= total; seq++) {
                for (k = i; k < n; k++) {
                    const char *t; long m;
                    if (used[k] || !msgs[k].concat) continue;
                    if (msgs[k].concat_ref != ref) continue;
                    if (strcmp(msgs[k].number, msgs[i].number) != 0) continue;
                    if (msgs[k].concat_seq != seq) continue;
                    for (t = msgs[k].text, m = 0; t[m] && tl + 1 < (long)sizeof j->text; m++)
                        j->text[tl++] = t[m];
                    j->text[tl] = 0;
                    if (seq == 1) hdr = k;
                    used[k] = 1; got++; break;
                }
            }
            j->pdu = msgs[hdr].pdu;
            strncpy(j->number, msgs[hdr].number, sizeof j->number - 1);
            j->number[sizeof j->number - 1] = 0;
            j->has_time = msgs[hdr].has_time; j->time = msgs[hdr].time; j->parts = got;
            nout++;
        }
    }
    return nout;
}

/* The folder list. Returns the folder count. */
static int get_folders(nl_link *l, nl_sms_folder *folders, int max)
{
    nl_u8 req[16], reply[512]; long rn, len; int err, nf = 0;
    rn = nl_sms_req_folders(req, sizeof req);
    err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
    if (err == NL_ERR_TIMEOUT)
        err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
    if (err != NL_OK || len < 5 || reply[3] != 0x7B) return 0;
    if (nl_sms_parse_folders(reply, len, folders, max, &nf) != NL_SMS_OK) return 0;
    return nf;
}

/* The message locations in one folder. For the Inbox, the read (0x08) and
 * unread (0xF8) lists are both asked for and merged. Returns the count. */
static int folder_locations(nl_link *l, nl_u8 folderid, nl_u16 *locs, int max)
{
    nl_u8 req[16], reply[512]; long rn, len; int err, n = 0, extra = 0;
    rn = nl_sms_req_folder_status(req, sizeof req, folderid);
    err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
    if (err == NL_ERR_TIMEOUT)
        err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
    if (err != NL_OK || len < 6 || reply[3] != 0x6C) return 0;
    nl_sms_parse_folder_status(reply, len, locs, max, &n);
    if (folderid == NL_SMS_FOLDER_INBOX) {
        rn = nl_sms_req_folder_status(req, sizeof req, NL_SMS_FOLDER_INBOX_UNREAD);
        err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
        if (err == NL_OK && len >= 6 && reply[3] == 0x6C)
            nl_sms_parse_folder_status(reply, len, locs + n, max - n, &extra);
        n += extra;
    }
    return n;
}

/* Read the messages at the given locations in one folder, parts joined. */
static int read_folder_joined(nl_link *l, nl_u8 folderid, const nl_u16 *locs, int nloc,
                              joined_msg *out, int max)
{
    nl_u8 req[16], reply[512]; long rn, len;
    static nl_sms msgs[512];
    int err, n = 0, li;
    for (li = 0; li < nloc && n < 512; li++) {
        rn = nl_sms_req_read(req, sizeof req, folderid, locs[li]);
        err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
        if (err == NL_ERR_TIMEOUT)
            err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE, reply, sizeof reply, &len, 2500);
        if (err != NL_OK || len < 9 || reply[3] != SMS_READ_OK) continue;
        if (nl_sms_parse_message(reply, len, &msgs[n]) != NL_SMS_OK) continue;
        n++;
    }
    return join_msgs(msgs, n, out, max);
}

static void print_joined(int idx, const joined_msg *j)
{
    printf("  [%d] %s", idx, nl_sms_pdu_name(j->pdu));
    if (j->number[0])
        printf("  %s %s", j->pdu == NL_SMS_SUBMIT ? "to" : "from", j->number);
    if (j->has_time)
        printf("  %04d-%02d-%02d %02ld:%02ld",
               nl_date_year(j->time.date), nl_date_month(j->time.date),
               nl_date_day(j->time.date), j->time.secs / 3600, (j->time.secs / 60) % 60);
    if (j->parts > 1) printf("  (%d parts)", j->parts);
    printf("\n");
    if (j->text[0]) printf("       %s\n", j->text);
}

/* Read the counts, then every folder's messages (parts joined). */
static int do_sms(nl_link *l)
{
    nl_u8 req[16], reply[512]; long rn, len;
    nl_sms_status st;
    nl_sms_folder folders[NL_SMS_MAX_FOLDERS];
    static joined_msg out[512];
    static nl_u16 locs[1200];
    int err, i, k, n, nf, total = 0;

    rn = nl_sms_req_status(req, sizeof req);
    err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE,
                          reply, sizeof reply, &len, 2500);
    if (err == NL_ERR_TIMEOUT)
        err = nl_link_request(l, NL_SMS_MSG_TYPE, req, rn, NL_SMS_MSG_TYPE,
                              reply, sizeof reply, &len, 2500);
    if (err != NL_OK) { printf("sms status: %s\n", error_text(err)); return 1; }
    if (nl_sms_parse_status(reply, len, &st) != NL_SMS_OK) {
        hex("sms status (undecoded)", reply, len); return 1;
    }
    printf("messages: phone %u (unread %u), SIM %u (unread %u), SIM size %u\n",
           (unsigned)st.phone_used, (unsigned)st.phone_unread,
           (unsigned)st.sim_used, (unsigned)st.sim_unread, (unsigned)st.sim_size);

    nf = get_folders(l, folders, NL_SMS_MAX_FOLDERS);
    if (nf == 0) { printf("no folders listed\n"); return 1; }
    for (i = 0; i < nf; i++) {
        int nloc = folder_locations(l, folders[i].id, locs, 1200);
        printf("%s (%d):\n", folders[i].name, nloc);
        n = read_folder_joined(l, folders[i].id, locs, nloc, out, 512);
        for (k = 0; k < n; k++) print_joined(k + 1, &out[k]);
        total += n;
    }
    printf("read %d messages across %d folders\n", total, nf);
    return 0;
}

/* Append every folder's messages to the archive journal, skipping any
 * already in it (by fingerprint). Only ever adds; never rewrites or deletes. */
#define ARC_MAX_FP 20000
static int do_archive_sms(nl_link *l, const char *path)
{
    static joined_msg out[512];
    static nl_u16 locs[1200];
    static char fps[ARC_MAX_FP][9];
    static char line[8192];
    nl_sms_folder folders[NL_SMS_MAX_FOLDERS];
    FILE *f;
    int nfps = 0, nf, fi, n, i, added = 0, dup = 0, isnew = 0, total = 0;

    /* Load the fingerprints already in the file. */
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            if (strlen(line) >= 9 && line[8] == '\t' && nfps < ARC_MAX_FP) {
                memcpy(fps[nfps], line, 8); fps[nfps][8] = 0; nfps++;
            }
        }
        fclose(f);
    } else {
        isnew = 1;
    }

    nf = get_folders(l, folders, NL_SMS_MAX_FOLDERS);
    if (nf == 0) { printf("archive: no folders listed\n"); return 1; }

    f = fopen(path, "a");
    if (!f) { printf("archive: cannot open %s for writing\n", path); return 1; }
    if (isnew) fputs(NL_MSGARC_HEADER, f);

    for (fi = 0; fi < nf; fi++) {
        int nloc = folder_locations(l, folders[fi].id, locs, 1200);
        n = read_folder_joined(l, folders[fi].id, locs, nloc, out, 512);
        total += n;
        for (i = 0; i < n; i++) {
            nl_msg_rec r;
            static char rec[8192];
            char fphex[9];
            long rl; int j, seen = 0;

            memset(&r, 0, sizeof r);
            r.folder = folders[fi].name;
            r.incoming = (out[i].pdu != NL_SMS_SUBMIT);
            r.number = out[i].number;
            r.name = "";
            r.date = out[i].has_time ? out[i].time.date : 0;
            r.secs = out[i].time.secs;
            r.has_time = out[i].has_time;
            r.parts = out[i].parts;
            r.text = out[i].text;

            sprintf(fphex, "%08lx", (unsigned long)nl_msgarc_fingerprint(&r));
            for (j = 0; j < nfps; j++)
                if (strcmp(fps[j], fphex) == 0) { seen = 1; break; }
            if (seen) { dup++; continue; }

            rl = nl_msgarc_format(&r, rec, sizeof rec);
            if (rl < 0) { printf("archive: a message was too long, skipped\n"); continue; }
            fputs(rec, f);
            if (nfps < ARC_MAX_FP) { strcpy(fps[nfps++], fphex); }  /* guard dups within this run */
            added++;
        }
    }
    fclose(f);
    printf("archive: %d messages across %d folders; added %d new, %d already present -> %s\n",
           total, nf, added, dup, path);
    return 0;
}

static void print_note(const nl_cal_note *n)
{
    printf("  [%u] %s", (unsigned)n->location, nl_cal_type_name(n->type));
    if (n->date)
        printf("  %04d-%02d-%02d", nl_date_year(n->date), nl_date_month(n->date), nl_date_day(n->date));
    if (n->type == NL_CAL_MEETING || n->type == NL_CAL_CALL)
        printf(" %02d:%02d", n->hour, n->minute);
    if (n->type == NL_CAL_BIRTHDAY && n->birth_year)
        printf("  (b. %d)", n->birth_year);
    if (n->recur != NL_REC_NONE)
        printf("  repeat %s", nl_cal_recur_name(n->recur));
    if (n->has_alarm)
        printf("  +alarm");
    printf("\n");
    if (n->text[0])   printf("       %s\n", n->text);
    if (n->number[0]) printf("       %s\n", n->number);
}

/* List the used note slots (paged), then read each note. */
static int do_calendar(nl_link *l)
{
    nl_u8 req[16], reply[512]; long rn, len;
    nl_u16 total = 0, locs[NL_CAL_MAX_LOCS];
    static nl_u16 all[1024];
    int nlocs = 0, err, got = 0, nall = 0, i, page;
    nl_u16 after = 0;

    for (page = 0; page < 32; page++) {
        rn = nl_cal_req_notes_info(req, sizeof req, after);
        err = nl_link_request(l, NL_CAL_MSG_TYPE, req, rn, NL_CAL_MSG_TYPE,
                              reply, sizeof reply, &len, 2500);
        if (err == NL_ERR_TIMEOUT && page == 0)   /* first request can be lost */
            err = nl_link_request(l, NL_CAL_MSG_TYPE, req, rn, NL_CAL_MSG_TYPE,
                                  reply, sizeof reply, &len, 2500);
        if (err != NL_OK) { printf("calendar info: %s\n", error_text(err)); return 1; }
        err = nl_cal_parse_notes_info(reply, len, &total, locs, NL_CAL_MAX_LOCS, &nlocs);
        if (err == NL_CAL_ERR_EMPTY) { printf("calendar: no notes\n"); return 0; }
        if (err != NL_OK) { hex("notes-info (undecoded)", reply, len); return 1; }
        for (i = 0; i < nlocs && nall < (int)(sizeof all / sizeof all[0]); i++)
            all[nall++] = locs[i];
        if (nlocs == 0 || nall >= (int)total) break;
        after = locs[nlocs - 1];
    }
    printf("calendar: %u notes\n", (unsigned)total);

    for (i = 0; i < nall; i++) {
        nl_cal_note note;
        rn = nl_cal_req_get_note(req, sizeof req, all[i]);
        err = nl_link_request(l, NL_CAL_MSG_TYPE, req, rn, NL_CAL_MSG_TYPE,
                              reply, sizeof reply, &len, 2500);
        if (err != NL_OK) { printf("  [%u] %s\n", (unsigned)all[i], error_text(err)); continue; }
        err = nl_cal_parse_note(reply, len, &note);
        if (err != NL_OK) { hex("  note (undecoded)", reply, len); continue; }
        print_note(&note);
        got++;
    }
    printf("read %d of %u notes\n", got, (unsigned)total);
    return 0;
}

/* One call register (dialled/missed/received): read slots until they run
 * out. Each entry carries a number, maybe a matched name, and the time. */
static int read_register(nl_link *l, nl_u8 mem)
{
    nl_u8 req[32], reply[512]; long rn, len, loc;
    int err, found = 0, miss = 0;
    const char *name = nl_pbk_mem_name(mem);

    printf("%s:\n", name);
    for (loc = 1; loc <= 30 && miss < 3; loc++) {
        nl_pbk_entry e;
        rn = nl_pbk_req_read(req, sizeof req, mem, (nl_u16)loc);
        if (rn < 0) break;
        err = nl_link_request(l, NL_PBK_MSG_TYPE, req, rn, NL_PBK_MSG_TYPE,
                              reply, sizeof reply, &len, 2500);
        if (err == NL_ERR_TIMEOUT && loc == 1)   /* first request can be lost */
            err = nl_link_request(l, NL_PBK_MSG_TYPE, req, rn, NL_PBK_MSG_TYPE,
                                  reply, sizeof reply, &len, 2500);
        if (err != NL_OK) { miss++; continue; }
        err = nl_pbk_parse_entry(reply, len, (nl_u16)loc, &e);
        if (err == NL_PBK_ERR_EMPTY) { miss++; continue; }
        if (err != NL_PBK_OK) { hex("  entry (undecoded)", reply, len); miss++; continue; }
        /* A real call entry always carries the time of the call; reading
         * past the end of a register returns a leftover entry without one,
         * so treat "no time" as the end. */
        if (!e.has_when) break;

        printf("  [%u]", (unsigned)e.location);
        if (e.has_when)
            printf(" %04d-%02d-%02d %02ld:%02ld",
                   nl_date_year(e.when.date), nl_date_month(e.when.date), nl_date_day(e.when.date),
                   e.when.secs / 3600, (e.when.secs / 60) % 60);
        if (e.nnumbers) printf("  %s", e.numbers[0].text);
        if (e.name[0])  printf("  %s", e.name);
        printf("\n");
        found++; miss = 0;
    }
    printf("  %d entries\n", found);
    return 0;
}

/* The three call registers. */
static int do_calls(nl_link *l)
{
    int bad = 0;
    bad |= read_register(l, NL_PBK_MEM_DIALLED);
    bad |= read_register(l, NL_PBK_MEM_MISSED);
    bad |= read_register(l, NL_PBK_MEM_RECEIVED);
    return bad;
}

int main(int argc, char **argv)
{
    posix_port pp;
    nl_link link;
    const char *cmd;
    int err, bad = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: nokialink-host <command> <device> [trace-file]\n"
                        "  commands: identify battery signal network clock contacts sms calendar calls status\n"
                        "           archive-sms <device> <archive-file>\n");
        return 2;
    }
    cmd = argv[1];
    if (posix_port_open(&pp, argv[2]) != 0) {
        fprintf(stderr, "can't open %s: %s\n", argv[2], strerror(errno));
        return 1;
    }
    if (strcmp(cmd, "archive-sms") == 0) {
        if (argc < 4) {
            fprintf(stderr, "usage: nokialink-host archive-sms <device> <archive-file>\n");
            posix_port_close(&pp);
            return 2;
        }
    } else if (argc > 3) {
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
    else if (strcmp(cmd, "contacts") == 0) bad = do_contacts(&link);
    else if (strcmp(cmd, "sms")      == 0) bad = do_sms(&link);
    else if (strcmp(cmd, "calendar") == 0) bad = do_calendar(&link);
    else if (strcmp(cmd, "calls")    == 0) bad = do_calls(&link);
    else if (strcmp(cmd, "archive-sms") == 0) bad = do_archive_sms(&link, argv[3]);
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

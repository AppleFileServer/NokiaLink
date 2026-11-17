/* Phone status and clock over the cable.
 *
 * Builds the request bodies and decodes the replies for the Phone
 * window and the clock rules: the model and firmware, the IMEI, the
 * battery level, the signal strength, the network, and the date and
 * time. Read-only, as Phase 1 asks.
 *
 * Each request is a plain body; the caller sends it with the FBUS frame
 * type named beside it below (they differ per request).
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation. Layout
 * from the gnokii description and the firmware; no gnokii code.
 */
#ifndef NL_INFO_H
#define NL_INFO_H

#include "../store/nltext.h"
#include "../store/nldate.h"

/* FBUS frame types for each request (passed to the link layer). */
#define NL_INFO_TYPE_IDENTIFY  0xD1
#define NL_INFO_TYPE_IMEI      0x40
#define NL_INFO_TYPE_BATTERY   0x17
#define NL_INFO_TYPE_SIGNAL    0x0A
#define NL_INFO_TYPE_NETWORK   0x0A
#define NL_INFO_TYPE_CLOCK     0x19

#define NL_INFO_OK          0
#define NL_INFO_ERR_SHORT  (-1)
#define NL_INFO_ERR_SPACE  (-2)
#define NL_INFO_ERR_SUB    (-3)
#define NL_INFO_ERR_NODATA (-4)   /* the reply carries no value (e.g. clock not set) */

typedef struct {
    char model[16];     /* e.g. "NSE-5" */
    char version[24];   /* firmware, e.g. "04.80" */
    char date[16];      /* firmware date, e.g. "23-02-00" */
} nl_info_ident;

typedef struct {
    int  registered;    /* 1 if camped on a network */
    char code[12];      /* MCC MNC, e.g. "234 15" */
    char lac[8];        /* location area code, hex */
    char cid[8];        /* cell id, hex */
    char name[64];      /* operator name, Mac Roman */
    long name_lost;
} nl_info_network;

/* ---- Requests (build a body) ---------------------------------------- */
long nl_info_req_identify(nl_u8 *out, long outsize);
long nl_info_req_imei(nl_u8 *out, long outsize);
long nl_info_req_battery(nl_u8 *out, long outsize);
long nl_info_req_signal(nl_u8 *out, long outsize);
long nl_info_req_network(nl_u8 *out, long outsize);
long nl_info_req_clock(nl_u8 *out, long outsize);

/* ---- Replies -------------------------------------------------------- */
int nl_info_parse_identify(const nl_u8 *body, long len, nl_info_ident *out);
int nl_info_parse_imei(const nl_u8 *body, long len, char *out, long outsize);
int nl_info_parse_battery(const nl_u8 *body, long len, int *percent);
int nl_info_parse_signal(const nl_u8 *body, long len, int *percent);
int nl_info_parse_network(const nl_u8 *body, long len, nl_info_network *out);
int nl_info_parse_clock(const nl_u8 *body, long len, nl_stamp *out);

#endif

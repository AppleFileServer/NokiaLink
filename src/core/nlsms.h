/* Text messages over the cable (FBUS message type 0x14).
 *
 * Builds the request bodies and decodes the replies for reading the
 * phone's messages: the message counts (status), and one stored message
 * from a folder. The message content is decoded with the GSM codec
 * (nlgsm): sender/service-centre numbers, the time, and the text in
 * whichever coding the message used. Read-only, as Phase 1 asks.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation. Layout
 * from the gnokii description and the GSM standards; no other project's
 * code.
 */
#ifndef NL_SMS_H
#define NL_SMS_H

#include "nlgsm.h"

#define NL_SMS_MSG_TYPE   0x14

#define NL_SMS_TEXT_MAX   360   /* Mac Roman text + zero (one SMS is <= 160) */
#define NL_SMS_NUM_MAX    48

#define NL_SMS_FOLDER_INBOX  0x08   /* folders are multiples of 0x08 */

#define NL_SMS_OK          0
#define NL_SMS_ERR_SHORT  (-1)
#define NL_SMS_ERR_SPACE  (-2)
#define NL_SMS_ERR_TYPE   (-3)

typedef enum { NL_SMS_DELIVER = 0, NL_SMS_SUBMIT = 1, NL_SMS_REPORT = 2 } nl_sms_pdu;
typedef enum { NL_SMS_7BIT = 0, NL_SMS_8BIT = 1, NL_SMS_UCS2 = 2 } nl_sms_coding;

typedef struct {
    nl_sms_pdu    pdu;
    int           state;        /* the phone's state byte (read/unread/sent/unsent) */
    int           udh;          /* 1 if the message carries a user-data header */
    nl_sms_coding coding;

    char          number[NL_SMS_NUM_MAX];  /* sender (deliver) or recipient (submit) */
    long          number_lost;
    char          smsc[NL_SMS_NUM_MAX];    /* service centre */

    int           has_time;     /* deliver messages carry a time */
    nl_stamp      time;
    int           tz_quarters;

    char          text[NL_SMS_TEXT_MAX];
    long          text_lost;
} nl_sms;

typedef struct {
    nl_u16 sim_size;
    nl_u16 phone_used, phone_unread;
    nl_u16 sim_used, sim_unread;
} nl_sms_status;

/* ---- Requests ------------------------------------------------------- */

long nl_sms_req_status(nl_u8 *out, long outsize);
long nl_sms_req_read(nl_u8 *out, long outsize, nl_u8 folder, nl_u16 location);

/* ---- Replies -------------------------------------------------------- */

int nl_sms_parse_status(const nl_u8 *body, long len, nl_sms_status *st);

/* Decode a stored message into *out (cleared first). Returns NL_SMS_OK,
 * NL_SMS_ERR_TYPE for a kind not handled, or an error. The message body
 * begins at offset 9; this reads the state at offset 4. */
int nl_sms_parse_message(const nl_u8 *body, long len, nl_sms *out);

const char *nl_sms_pdu_name(nl_sms_pdu p);

#endif

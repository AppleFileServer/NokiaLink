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
#define NL_SMS_SMSC_TYPE  0x02   /* the message-centre request/reply use type 0x02 */

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

    /* Concatenation (a long message split into parts), from the UDH. */
    int           concat;       /* 1 if this is one part of a joined message */
    int           concat_ref;   /* reference shared by the parts (8- or 16-bit) */
    int           concat_total; /* how many parts in all */
    int           concat_seq;   /* which part this is, 1..concat_total */

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

#define NL_SMS_MAX_FOLDERS      16
#define NL_SMS_FOLDER_NAME_MAX  40
#define NL_SMS_FOLDER_INBOX_UNREAD 0xF8  /* the Inbox's unread messages */

typedef struct {
    nl_u8 id;                              /* folder id, for status/read requests */
    char  name[NL_SMS_FOLDER_NAME_MAX];    /* Mac Roman */
    long  name_lost;
} nl_sms_folder;

/* ---- Requests ------------------------------------------------------- */

long nl_sms_req_status(nl_u8 *out, long outsize);
long nl_sms_req_read(nl_u8 *out, long outsize, nl_u8 folder, nl_u16 location);

/* List the message folders, and the message locations within one folder. */
long nl_sms_req_folders(nl_u8 *out, long outsize);
long nl_sms_req_folder_status(nl_u8 *out, long outsize, nl_u8 folderid);

/* The message-centre (SMSC) at a location (1 is the usual one); type 0x02. */
long nl_sms_req_smsc(nl_u8 *out, long outsize, nl_u8 location);

/* Delete the message at folderid/location (Phase 2). The phone answers
 * type 0x14 sub NL_SMS_DELETE_OK1 or _OK2. */
#define NL_SMS_DELETE_OK1  0x0B
#define NL_SMS_DELETE_OK2  0x0C
long nl_sms_req_delete(nl_u8 *out, long outsize, nl_u8 folderid, nl_u16 location);

/* Send a text (SMS-SUBMIT) to `dest` via service centre `smsc`, 7-bit text.
 * Sent with type NL_SMS_SMSC_TYPE (0x02); the reply is sub NL_SMS_SEND_OK
 * (sent) or NL_SMS_SEND_FAIL (with an error code). Returns the body length. */
#define NL_SMS_SEND_OK    0x02
#define NL_SMS_SEND_FAIL  0x03
long nl_sms_req_send(nl_u8 *out, long outsize, const char *smsc,
                     const char *dest, const char *text);
int  nl_sms_parse_send(const nl_u8 *body, long len, int *ref, int *err_code);

/* ---- Replies -------------------------------------------------------- */

int nl_sms_parse_status(const nl_u8 *body, long len, nl_sms_status *st);

/* Decode the folder-list reply (sub 0x7B): each folder's id and name. */
int nl_sms_parse_folders(const nl_u8 *body, long len, nl_sms_folder *out, int max, int *n);

/* Decode a folder-status reply (sub 0x6C): the message locations in the
 * folder into locs[0..*n-1]. */
int nl_sms_parse_folder_status(const nl_u8 *body, long len, nl_u16 *locs, int max, int *n);

/* Decode the SMSC reply (sub 0x34): the centre's number and its name. */
int nl_sms_parse_smsc(const nl_u8 *body, long len,
                      char *number, long numsize, char *name, long namesize);

/* Decode a stored message into *out (cleared first). Returns NL_SMS_OK,
 * NL_SMS_ERR_TYPE for a kind not handled, or an error. The message body
 * begins at offset 9; this reads the state at offset 4. */
int nl_sms_parse_message(const nl_u8 *body, long len, nl_sms *out);

const char *nl_sms_pdu_name(nl_sms_pdu p);

#endif

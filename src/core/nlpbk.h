/* Contacts over the cable (FBUS message type 0x03, "phonebook").
 *
 * Builds the request bodies and decodes the replies for reading the
 * phone's contacts: the memory status (how many entries used and free),
 * and one entry by memory and location. Read-only, as Phase 1 asks.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation. The
 * layout is from the gnokii project's description and the firmware map;
 * no gnokii code is used.
 *
 * An entry is a run of typed sub-blocks (a name, several numbers, an
 * email, a postal address, a note). Each text comes off the phone as
 * UCS-2 and is converted to the store's Mac Roman (nltext); a character
 * Mac Roman lacks becomes '?' and is counted, never dropped (CON-3).
 */
#ifndef NL_PBK_H
#define NL_PBK_H

#include "../store/nltext.h"

#define NL_PBK_MSG_TYPE   0x03

#define NL_PBK_MEM_ME     0x05   /* the phone's own memory (several numbers, email, ...) */
#define NL_PBK_MEM_SIM    0x06   /* the SIM card (a name and one number) */

#define NL_PBK_TEXT_MAX   128    /* Mac Roman name/email/postal/note + zero */
#define NL_PBK_NUM_MAX    48     /* a phone number + zero */
#define NL_PBK_MAX_NUMBERS 8     /* numbers kept per entry */

#define NL_PBK_OK          0
#define NL_PBK_ERR_SHORT  (-1)   /* body shorter than its fields need */
#define NL_PBK_ERR_SPACE  (-2)   /* caller's buffer too small */
#define NL_PBK_ERR_EMPTY  (-3)   /* no entry at this location */
#define NL_PBK_ERR_MEM    (-4)   /* memory type is not ME or SIM */

typedef enum {
    NL_NUM_GENERAL = 0, NL_NUM_HOME, NL_NUM_MOBILE, NL_NUM_FAX, NL_NUM_WORK
} nl_num_type;

typedef struct {
    nl_num_type type;
    char        text[NL_PBK_NUM_MAX];   /* Mac Roman */
    long        lost;
} nl_pbk_number;

typedef struct {
    nl_u16        location;
    char          name[NL_PBK_TEXT_MAX];   long name_lost;
    char          email[NL_PBK_TEXT_MAX];  long email_lost;
    char          postal[NL_PBK_TEXT_MAX]; long postal_lost;
    char          note[NL_PBK_TEXT_MAX];   long note_lost;
    nl_pbk_number numbers[NL_PBK_MAX_NUMBERS];
    int           nnumbers;
} nl_pbk_entry;

/* ---- Requests ------------------------------------------------------- */

/* Read one entry from `memtype` (NL_PBK_MEM_ME or _SIM) at `location`.
 * Returns the body length, NL_PBK_ERR_SPACE, or NL_PBK_ERR_MEM. */
long nl_pbk_req_read(nl_u8 *out, long outsize, nl_u8 memtype, nl_u16 location);

/* Ask how many entries a memory holds and how many are free. */
long nl_pbk_req_status(nl_u8 *out, long outsize, nl_u8 memtype);

/* ---- Replies -------------------------------------------------------- */

/* Decode the memory-status reply: *used and *total (capacity). */
int nl_pbk_parse_status(const nl_u8 *body, long len, nl_u16 *used, nl_u16 *total);

/* Decode an entry reply into *e (all fields cleared first). Returns
 * NL_PBK_OK, NL_PBK_ERR_EMPTY when the slot holds no entry, or an error.
 * `location` is the slot that was asked for (the reply does not repeat
 * it reliably), stored into e->location. */
int nl_pbk_parse_entry(const nl_u8 *body, long len, nl_u16 location, nl_pbk_entry *e);

const char *nl_num_type_name(nl_num_type t);

#endif

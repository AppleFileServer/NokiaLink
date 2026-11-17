/* Contacts sync: turn the three-way compare's raw verbs into the plan of
 * work, applying the owner's policy (SPECIFICATION.md section 4.3,
 * decisions 8, 10, 11).
 *
 * Sync runs on the Main list only (decision 8): the caller passes the
 * Main contacts of each side (and the base) to the compare, then this
 * maps each verb to a contacts operation:
 *
 *   new on phone            -> add to the store's Main
 *                              (but if its number matches an Archived
 *                               contact, revive that one instead -> dec. 11)
 *   new in the store        -> write to the phone
 *   changed on phone        -> update the store
 *   changed in the store    -> write to the phone
 *   gone from the store Main -> remove from the phone (dec. 8)
 *   gone from the phone     -> move the store card to Archive (dec. 10),
 *                              never delete it
 *   changed on both         -> conflict, for the owner to settle (CON-5)
 *
 * Pure logic, no allocation; it builds a plan, it does not touch the
 * store or the phone.
 */
#ifndef NL_CTSYNC_H
#define NL_CTSYNC_H

#include "nlsync.h"

#define NL_CT_OK          0
#define NL_CT_ERR_SPACE  (-1)

typedef enum {
    NL_CT_ADD_TO_STORE = 0,   /* new phone contact -> add to store Main */
    NL_CT_ADD_TO_PHONE,       /* new store contact -> write to phone */
    NL_CT_UPDATE_STORE,
    NL_CT_UPDATE_PHONE,
    NL_CT_REMOVE_FROM_PHONE,  /* gone from store Main */
    NL_CT_MOVE_TO_ARCHIVE,    /* gone from phone (never deleted) */
    NL_CT_UNARCHIVE,          /* new phone contact matches an archived one */
    NL_CT_CONFLICT
} nl_ct_op;

typedef struct {
    nl_ct_op op;
    char     key[NL_SYNC_KEY_MAX];    /* the contact this is about (phone/store key) */
    char     other[NL_SYNC_KEY_MAX];  /* UNARCHIVE: the archived contact to revive */
} nl_ct_action;

/* Decide whether a new-on-phone contact matches an existing Archived
 * contact (by number). Returns 1 and fills archived_key_out if so, else
 * 0. The caller implements the number matching over its own data; may be
 * null (then no un-archiving happens). */
typedef int (*nl_ct_archive_match)(void *ctx, const char *phone_key,
                                   char *archived_key_out, long out_size);

/* The base verb-to-operation mapping (no decision-11 refinement). */
nl_ct_op nl_ct_policy(nl_sync_verb v);

/* Map the compare's actions into the contacts plan, applying policy and
 * (for a new phone contact) the archive-match refinement. Returns NL_CT_OK
 * or NL_CT_ERR_SPACE. */
int nl_ct_map(const nl_sync_action *in, int nin,
              nl_ct_archive_match match, void *ctx,
              nl_ct_action *out, int outmax, int *nout);

const char *nl_ct_op_name(nl_ct_op op);

#endif

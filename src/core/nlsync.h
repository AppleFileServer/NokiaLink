/* Three-way compare for sync (SPECIFICATION.md CON-5, CAL-3).
 *
 * The phone keeps no record of what changed, so sync compares three
 * sides: the phone now, the store now, and the copy saved at the last
 * sync (the "base"). A change on one side only is carried to the other;
 * a change on both is a conflict for the owner to settle.
 *
 * This engine is generic. Each side is a set of items, each with a
 * stable key (a contact's id, say) and a hash of its content (so only
 * equality is needed here; the caller fingerprints the content). The
 * engine returns one action per key that needs something done. The
 * domain layer turns those into real work and applies its own policy
 * (for contacts, a deletion on the phone becomes "move to Archive", and
 * so on): the verbs below are the raw merge result, not the policy.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation.
 */
#ifndef NL_SYNC_H
#define NL_SYNC_H

#ifndef NL_BASIC_TYPES
#define NL_BASIC_TYPES
typedef unsigned char  nl_u8;
typedef unsigned short nl_u16;
typedef unsigned long  nl_u32;
#endif

#define NL_SYNC_KEY_MAX  64

#define NL_SYNC_OK         0
#define NL_SYNC_ERR_SPACE (-1)   /* more actions than the caller's buffer holds */

typedef struct {
    const char *key;
    nl_u32      hash;
} nl_sync_item;

typedef enum {
    NL_SYNC_NONE = 0,
    NL_SYNC_ADD_TO_STORE,       /* new on the phone -> copy into the store */
    NL_SYNC_ADD_TO_PHONE,       /* new in the store -> copy to the phone */
    NL_SYNC_UPDATE_STORE,       /* changed on the phone -> update the store */
    NL_SYNC_UPDATE_PHONE,       /* changed in the store -> update the phone */
    NL_SYNC_DELETE_FROM_PHONE,  /* removed from the store -> remove from the phone */
    NL_SYNC_DELETE_FROM_STORE,  /* removed from the phone -> remove from the store */
    NL_SYNC_CONFLICT            /* changed on both sides, differently */
} nl_sync_verb;

typedef struct {
    char         key[NL_SYNC_KEY_MAX];
    nl_sync_verb verb;
} nl_sync_action;

/* Compare the three sides. Writes one action per key that needs work
 * (keys needing nothing are left out) to out[0..*nout-1]. Returns
 * NL_SYNC_OK, or NL_SYNC_ERR_SPACE if there are more than outmax. The
 * three arrays need not be sorted. */
int nl_sync_compare(const nl_sync_item *base,  int nbase,
                    const nl_sync_item *phone, int nphone,
                    const nl_sync_item *store, int nstore,
                    nl_sync_action *out, int outmax, int *nout);

/* Decide the verb for one key from its presence and hash on each side.
 * present_* is 0 or 1; the hash is read only where present. Exposed for
 * testing and reuse. */
nl_sync_verb nl_sync_decide(int in_base,  nl_u32 hbase,
                            int in_phone, nl_u32 hphone,
                            int in_store, nl_u32 hstore);

const char *nl_sync_verb_name(nl_sync_verb v);

#endif

/* A contact's sync identity: a stable key and a content fingerprint.
 *
 * Sync's three-way compare works on (key, hash) items. For a contact the
 * key is its vCard UID (stable across syncs), and the hash is a CRC-32
 * over the meaningful fields only: name, numbers, emails, address,
 * organisation and note. It deliberately ignores the UID itself, the
 * last-used and kept-on dates (which change without being an edit), the
 * order of lines, how a number is punctuated, and the case of an email,
 * so only a real change to the contact alters the hash.
 *
 * Portable C, no Macintosh or POSIX calls, no allocation.
 */
#ifndef NL_CONTACT_H
#define NL_CONTACT_H

#include "nlvcard.h"
#include "nlcrc.h"

#define NL_CONTACT_OK          0
#define NL_CONTACT_ERR_NOKEY  (-1)   /* no UID and no name to key on */

/* The contact's key (its UID, or its FN if it somehow has no UID) into
 * `out`. Returns the length, or NL_CONTACT_ERR_NOKEY. */
long nl_contact_key(const nl_vcard *v, char *out, long outsize);

/* A fingerprint of the contact's meaningful content (see above). */
nl_u32 nl_contact_hash(const nl_vcard *v);

#endif

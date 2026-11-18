/* Build a stored contact card (a vCard 2.1) from a contact read off the
 * phone (an nl_pbk_entry). This is how a phone contact becomes a file in
 * the store's Contacts/Main or Contacts/Archive folder (SPEC decision 7,
 * section 6.1/6.2): N, FN, the numbers as TEL lines, e-mail, note, the
 * CATEGORIES line, NokiaLink's X- lines (memory, last-used, kept-on) and a
 * stable UID.
 *
 * Portable C, no Macintosh or POSIX calls, no allocation. Builds into an
 * nl_vcard (text form); the caller writes it to a file.
 */
#ifndef NL_CTVCARD_H
#define NL_CTVCARD_H

#include "nlvcard.h"
#include "nldate.h"
#include "../core/nlpbk.h"

/* Build card v from entry e.
 *  category   "Main" or "Archive" (written as CATEGORIES:card,<category>)
 *  memory     NL_PBK_MEM_ME -> X-NOKIA-MEMORY:PHONE, _SIM -> :SIM; else omitted
 *  last_used  YYYYMMDD, or 0 to omit X-LAST-USED
 *  kept_on    YYYYMMDD, or 0 to write X-KEPT-ON empty (never kept)
 *  uid        the contact's stable UID (must not be empty)
 * Returns NL_VC_OK, or NL_VC_TOO_LONG if the card does not fit. */
int nl_ctvcard_build(nl_vcard *v, const nl_pbk_entry *e, const char *category,
                     nl_u8 memory, nl_date last_used, nl_date kept_on,
                     const char *uid);

#endif

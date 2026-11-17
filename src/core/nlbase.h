/* The last-sync base: what the two sides agreed on at the end of the
 * previous sync, kept so the next sync has a common ancestor to compare
 * against (the third side of the three-way compare, nlsync).
 *
 * Held as a set of (key, content-hash) pairs and saved as plain text in
 * the store (programme P32): a header line, then one "key<tab>hash" line
 * per item, Mac Roman with CR line ends. The store keeps its own file
 * checksum (nlcrc) separately.
 *
 * Portable C, no Macintosh or POSIX calls, no allocation. The caller
 * owns the nl_base (it is large; put it in a global, not on the stack).
 */
#ifndef NL_BASE_H
#define NL_BASE_H

#include "nlsync.h"

#define NL_BASE_MAX  1024    /* items the base can hold */

#define NL_BASE_OK          0
#define NL_BASE_ERR_FULL   (-1)
#define NL_BASE_ERR_SPACE  (-2)   /* write buffer too small */
#define NL_BASE_ERR_FORMAT (-3)   /* not a base file */

typedef struct {
    int    n;
    char   key[NL_BASE_MAX][NL_SYNC_KEY_MAX];
    nl_u32 hash[NL_BASE_MAX];
} nl_base;

void nl_base_clear(nl_base *b);

/* Add or replace an item. Returns NL_BASE_OK or NL_BASE_ERR_FULL. */
int  nl_base_add(nl_base *b, const char *key, nl_u32 hash);

/* 1 and sets *hash if the key is present, else 0. */
int  nl_base_find(const nl_base *b, const char *key, nl_u32 *hash);

/* Fill an nl_sync_item view for nl_sync_compare. The key pointers point
 * into the base's own storage, so the base must outlive the compare. */
void nl_base_items(const nl_base *b, nl_sync_item *items, int max, int *n);

/* Write the base as text. Returns the length, or NL_BASE_ERR_SPACE. */
long nl_base_write(const nl_base *b, char *out, long outsize);

/* Replace the base from text (any of CR, LF, CRLF line ends). */
int  nl_base_parse(nl_base *b, const char *text, long len);

#endif

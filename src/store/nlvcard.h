/* Reading and changing one contact card (a vCard) from the store.
 *
 * Portable C, no Macintosh or POSIX calls, no memory allocation.
 *
 * A card is kept as its own text and edited in place, so every line
 * NokiaLink doesn't know about is kept exactly as another program wrote
 * it (SPECIFICATION.md section 6.2). Line ends are CR inside the card
 * (the Plus's own); CR, LF and CRLF are all accepted on loading.
 *
 * A property is one line: NAME[;parameters]:value. A line that begins
 * with a space or tab continues the one before. A name may carry a
 * group ("item1.TEL"), which is ignored when looking a name up. Names
 * and parameters are matched without regard to case.
 */
#ifndef NL_VCARD_H
#define NL_VCARD_H

#define NL_VCARD_MAX   2048     /* the longest card held, in bytes */

#define NL_VC_OK         0
#define NL_VC_NOT_CARD  (-1)    /* no BEGIN:VCARD ... END:VCARD */
#define NL_VC_TOO_LONG  (-2)    /* the card, or a value, doesn't fit */
#define NL_VC_MISSING   (-3)    /* no such property */

typedef struct {
    char text[NL_VCARD_MAX];    /* BEGIN:VCARD\r ... END:VCARD\r, then a zero byte */
    long len;
} nl_vcard;

/* A new empty card: BEGIN, VERSION:2.1, END. */
void nl_vcard_init(nl_vcard *v);

/* Loads the first card found in text (len bytes). */
int  nl_vcard_load(nl_vcard *v, const char *text, long len);

/* How many properties are called name. */
int  nl_vcard_count(const nl_vcard *v, const char *name);

/* The value of the index-th property called name (0 = the first),
 * copied to out with continuation lines joined. Returns its length, or
 * NL_VC_MISSING, or NL_VC_TOO_LONG if it doesn't fit outsize. */
long nl_vcard_get(const nl_vcard *v, const char *name, int index,
                  char *out, long outsize);

/* The parameters of that property (the text between the name and the
 * colon, without the leading semicolon; "" if none). Same returns. */
long nl_vcard_params(const nl_vcard *v, const char *name, int index,
                     char *out, long outsize);

/* 1 if the parameters hold the word (as "CELL" in "CELL;VOICE" or in
 * "type=CELL;type=VOICE"). */
int  nl_vcard_has_param(const char *params, const char *word);

/* Sets the value of the first property called name, keeping its
 * parameters; adds "NAME:value" before END if there is none. An empty
 * value is allowed ("X-KEPT-ON:"). */
int  nl_vcard_set(nl_vcard *v, const char *name, const char *value);

/* Adds a property before END. params may be 0 or "". */
int  nl_vcard_add(nl_vcard *v, const char *name, const char *params,
                  const char *value);

/* Removes the index-th property called name. */
int  nl_vcard_remove(nl_vcard *v, const char *name, int index);

/* Copies part number `part` (0 = the first) of a structured value such
 * as N's "Family;Given;;;" to out. A backslash before a semicolon keeps
 * it as part of the text. Returns the length, or NL_VC_TOO_LONG. A part
 * past the end is empty. */
long nl_vcard_part(const char *value, int part, char *out, long outsize);

#endif

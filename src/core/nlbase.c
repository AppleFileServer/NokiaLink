/* The last-sync base (see nlbase.h). */
#include "nlbase.h"

static const char HEADER[] = "NokiaLink base 1";

static int streq(const char *a, const char *b)
{ while (*a && *a == *b) { a++; b++; } return *a == *b; }

static void copykey(char *dst, const char *src)
{ int i = 0; while (i + 1 < NL_SYNC_KEY_MAX && src[i]) { dst[i] = src[i]; i++; } dst[i] = 0; }

void nl_base_clear(nl_base *b) { b->n = 0; }

int nl_base_add(nl_base *b, const char *key, nl_u32 hash)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (streq(b->key[i], key)) { b->hash[i] = hash; return NL_BASE_OK; }
    if (b->n >= NL_BASE_MAX) return NL_BASE_ERR_FULL;
    copykey(b->key[b->n], key);
    b->hash[b->n] = hash;
    b->n++;
    return NL_BASE_OK;
}

int nl_base_find(const nl_base *b, const char *key, nl_u32 *hash)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (streq(b->key[i], key)) { if (hash) *hash = b->hash[i]; return 1; }
    return 0;
}

void nl_base_items(const nl_base *b, nl_sync_item *items, int max, int *n)
{
    int i, m = b->n < max ? b->n : max;
    for (i = 0; i < m; i++) { items[i].key = b->key[i]; items[i].hash = b->hash[i]; }
    *n = m;
}

/* append a string / char to out, guarding capacity */
static int puts_(char *out, long cap, long *pos, const char *s)
{
    while (*s) { if (*pos >= cap) return 0; out[(*pos)++] = *s++; }
    return 1;
}
static int putc_(char *out, long cap, long *pos, char c)
{ if (*pos >= cap) return 0; out[(*pos)++] = c; return 1; }

static void hex8(nl_u32 v, char *out)
{
    static const char H[] = "0123456789ABCDEF";
    int i; for (i = 7; i >= 0; i--) { out[i] = H[v & 0xF]; v >>= 4; } out[8] = 0;
}

long nl_base_write(const nl_base *b, char *out, long outsize)
{
    long pos = 0;
    int i;
    char h[9];
    if (!puts_(out, outsize, &pos, HEADER) || !putc_(out, outsize, &pos, '\r'))
        return NL_BASE_ERR_SPACE;
    for (i = 0; i < b->n; i++) {
        hex8(b->hash[i], h);
        if (!puts_(out, outsize, &pos, b->key[i]) || !putc_(out, outsize, &pos, '\t') ||
            !puts_(out, outsize, &pos, h) || !putc_(out, outsize, &pos, '\r'))
            return NL_BASE_ERR_SPACE;
    }
    return pos;
}

static nl_u32 parse_hex(const char *s, int n)
{
    nl_u32 v = 0; int i;
    for (i = 0; i < n; i++) {
        char c = s[i]; int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else break;
        v = (v << 4) | (nl_u32)d;
    }
    return v;
}

int nl_base_parse(nl_base *b, const char *text, long len)
{
    long i = 0, start;
    int first = 1;
    nl_base_clear(b);
    while (i < len) {
        /* one line */
        start = i;
        while (i < len && text[i] != '\r' && text[i] != '\n') i++;
        {
            long linelen = i - start;
            const char *line = text + start;
            if (first) {
                /* header must match */
                long k; int ok = (linelen == (long)sizeof HEADER - 1);
                for (k = 0; ok && k < linelen; k++) if (line[k] != HEADER[k]) ok = 0;
                if (!ok) return NL_BASE_ERR_FORMAT;
                first = 0;
            } else if (linelen > 0) {
                /* key<tab>hash */
                long t = 0;
                while (t < linelen && line[t] != '\t') t++;
                if (t < linelen && t < NL_SYNC_KEY_MAX) {
                    char key[NL_SYNC_KEY_MAX]; long k;
                    for (k = 0; k < t; k++) key[k] = line[k];
                    key[t] = 0;
                    if (nl_base_add(b, key, parse_hex(line + t + 1, 8)) != NL_BASE_OK)
                        return NL_BASE_ERR_FULL;
                }
            }
        }
        /* step over the line end (CR, LF, or CRLF) */
        if (i < len && text[i] == '\r') i++;
        if (i < len && text[i] == '\n') i++;
    }
    if (first) return NL_BASE_ERR_FORMAT;   /* no header seen */
    return NL_BASE_OK;
}

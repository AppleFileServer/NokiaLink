/* One contact card (nlvcard.h). */
#include "nlvcard.h"

#define CR '\r'

static int upper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

static long zlen(const char *s) { long n = 0; while (s[n]) n++; return n; }

/* 1 if the n bytes at a equal the zero-ended word, without regard to case. */
static int word_is(const char *a, long n, const char *word)
{
    long i;
    for (i = 0; i < n; i++)
        if (!word[i] || upper(a[i]) != upper(word[i])) return 0;
    return word[n] == 0;
}

/* ---- Walking the properties -------------------------------------- */

typedef struct {
    long start, end;        /* the whole property with its line ends: text[start..end) */
    long name, name_len;    /* the name, without any group */
    long params, params_len;
    long value;             /* where the value begins (after the colon) */
} prop;

/* The property that begins at `at` (the start of a line). Returns 0 at
 * the end of the card. */
static int prop_at(const nl_vcard *v, long at, prop *p)
{
    const char *t = v->text;
    long i, colon = -1, semi = -1, dot = -1;

    if (at >= v->len) return 0;
    p->start = at;
    for (i = at; i < v->len && t[i] != CR; i++) {
        if (colon < 0) {
            if (t[i] == ':') colon = i;
            else if (t[i] == ';' && semi < 0) semi = i;
            else if (t[i] == '.' && semi < 0 && dot < 0) dot = i;
        }
    }
    /* Continuation lines belong to this property. */
    while (i < v->len) {
        long next = i + 1;
        if (next < v->len && (t[next] == ' ' || t[next] == '\t')) {
            for (i = next; i < v->len && t[i] != CR; i++) { }
        } else break;
    }
    p->end = i < v->len ? i + 1 : v->len;
    if (colon < 0) {                         /* not a property: a stray line */
        p->name = at; p->name_len = 0;
        p->params = at; p->params_len = 0;
        p->value = at;
        return 1;
    }
    p->name = dot >= 0 ? dot + 1 : at;
    p->name_len = (semi >= 0 ? semi : colon) - p->name;
    p->params = semi >= 0 ? semi + 1 : colon;
    p->params_len = colon - p->params;
    p->value = colon + 1;
    return 1;
}

/* Finds the index-th property called name. */
static int find(const nl_vcard *v, const char *name, int index, prop *p)
{
    long at = 0;
    while (prop_at(v, at, p)) {
        if (p->name_len > 0 && word_is(v->text + p->name, p->name_len, name)) {
            if (index == 0) return 1;
            index--;
        }
        at = p->end;
    }
    return 0;
}

/* ---- Editing the text -------------------------------------------- */

/* Replaces text[from..to) with the n bytes at with. */
static int splice(nl_vcard *v, long from, long to, const char *with, long n)
{
    long tail = v->len - to, shift = n - (to - from), i;

    if (v->len + shift > NL_VCARD_MAX - 1) return NL_VC_TOO_LONG;
    if (shift > 0)
        for (i = tail - 1; i >= 0; i--) v->text[to + shift + i] = v->text[to + i];
    else if (shift < 0)
        for (i = 0; i < tail; i++) v->text[to + shift + i] = v->text[to + i];
    for (i = 0; i < n; i++) v->text[from + i] = with[i];
    v->len += shift;
    v->text[v->len] = 0;
    return NL_VC_OK;
}

/* A value may not hold a line end: it would break the card in two. */
static int clean(const char *s)
{
    for (; *s; s++) if (*s == CR || *s == '\n') return 0;
    return 1;
}

void nl_vcard_init(nl_vcard *v)
{
    static const char empty[] = "BEGIN:VCARD\rVERSION:2.1\rEND:VCARD\r";
    long i;
    for (i = 0; empty[i]; i++) v->text[i] = empty[i];
    v->text[i] = 0;
    v->len = i;
}

int nl_vcard_load(nl_vcard *v, const char *text, long len)
{
    static const char begin[] = "BEGIN:VCARD", end[] = "END:VCARD";
    long i, start = -1, line = 0, out = 0;
    int ended = 0;

    v->len = 0;
    v->text[0] = 0;
    /* Find a line that is BEGIN:VCARD. */
    for (i = 0; i <= len; i++) {
        if (i == len || text[i] == CR || text[i] == '\n') {
            if (i - line == 11 && word_is(text + line, 11, begin)) { start = line; break; }
            line = i + 1;
        }
    }
    if (start < 0) return NL_VC_NOT_CARD;

    /* Copy line by line to the END line, turning every line end into CR
     * and dropping empty lines. */
    line = start;
    for (i = start; i <= len && !ended; i++) {
        if (i == len || text[i] == CR || text[i] == '\n') {
            long n = i - line, k;
            if (n > 0) {
                if (out + n + 1 > NL_VCARD_MAX - 1) { v->text[0] = 0; return NL_VC_TOO_LONG; }
                for (k = 0; k < n; k++) v->text[out++] = text[line + k];
                v->text[out++] = CR;
                if (n == 9 && word_is(text + line, 9, end)) ended = 1;
            }
            line = i + 1;
        }
    }
    if (!ended) { v->text[0] = 0; return NL_VC_NOT_CARD; }
    v->text[out] = 0;
    v->len = out;
    return NL_VC_OK;
}

int nl_vcard_count(const nl_vcard *v, const char *name)
{
    prop p;
    long at = 0;
    int n = 0;
    while (prop_at(v, at, &p)) {
        if (p.name_len > 0 && word_is(v->text + p.name, p.name_len, name)) n++;
        at = p.end;
    }
    return n;
}

long nl_vcard_get(const nl_vcard *v, const char *name, int index,
                  char *out, long outsize)
{
    prop p;
    long i, n = 0;

    if (outsize > 0) out[0] = 0;
    if (!find(v, name, index, &p)) return NL_VC_MISSING;
    for (i = p.value; i < p.end; i++) {
        char c = v->text[i];
        if (c == CR) {                       /* a line end: skip it and the fold's first space */
            if (i + 1 < p.end) i++;
            continue;
        }
        if (n + 1 >= outsize) { if (outsize > 0) out[0] = 0; return NL_VC_TOO_LONG; }
        out[n++] = c;
    }
    out[n] = 0;
    return n;
}

long nl_vcard_params(const nl_vcard *v, const char *name, int index,
                     char *out, long outsize)
{
    prop p;
    long i;

    if (outsize > 0) out[0] = 0;
    if (!find(v, name, index, &p)) return NL_VC_MISSING;
    if (p.params_len + 1 > outsize) return NL_VC_TOO_LONG;
    for (i = 0; i < p.params_len; i++) out[i] = v->text[p.params + i];
    out[i] = 0;
    return p.params_len;
}

int nl_vcard_has_param(const char *params, const char *word)
{
    long i = 0, start = 0;
    for (;; i++) {
        char c = params[i];
        if (c == '=') start = i + 1;                    /* "type=CELL": compare what follows */
        if (c == ';' || c == ',' || c == 0) {
            if (i > start && word_is(params + start, i - start, word)) return 1;
            if (c == 0) return 0;
            start = i + 1;
        }
    }
}

int nl_vcard_add(nl_vcard *v, const char *name, const char *params,
                 const char *value)
{
    prop p;
    char line[NL_VCARD_MAX];
    long n = 0, i;
    long nl = zlen(name), pl = params ? zlen(params) : 0, vl = zlen(value);

    if (!clean(name) || !clean(value) || (params && !clean(params))) return NL_VC_NOT_CARD;
    if (!find(v, "END", 0, &p)) return NL_VC_NOT_CARD;
    if (nl + pl + vl + 3 > NL_VCARD_MAX - 1) return NL_VC_TOO_LONG;
    for (i = 0; i < nl; i++) line[n++] = name[i];
    if (pl) { line[n++] = ';'; for (i = 0; i < pl; i++) line[n++] = params[i]; }
    line[n++] = ':';
    for (i = 0; i < vl; i++) line[n++] = value[i];
    line[n++] = CR;
    return splice(v, p.start, p.start, line, n);
}

int nl_vcard_set(nl_vcard *v, const char *name, const char *value)
{
    prop p;
    long end;

    if (!clean(value)) return NL_VC_NOT_CARD;
    if (!find(v, name, 0, &p)) return nl_vcard_add(v, name, 0, value);
    end = p.end;
    if (end > p.value && v->text[end - 1] == CR) end--;     /* keep the last line end */
    return splice(v, p.value, end, value, zlen(value));
}

int nl_vcard_remove(nl_vcard *v, const char *name, int index)
{
    prop p;
    if (!find(v, name, index, &p)) return NL_VC_MISSING;
    return splice(v, p.start, p.end, "", 0);
}

long nl_vcard_part(const char *value, int part, char *out, long outsize)
{
    long i, n = 0;

    if (outsize > 0) out[0] = 0;
    for (i = 0; value[i]; i++) {
        char c = value[i];
        if (c == '\\' && value[i + 1] == ';') {
            c = ';'; i++;
        } else if (c == ';') {
            if (part == 0) break;
            part--;
            continue;
        }
        if (part == 0) {
            if (n + 1 >= outsize) { if (outsize > 0) out[0] = 0; return NL_VC_TOO_LONG; }
            out[n++] = c;
        }
    }
    if (outsize > 0) out[n] = 0;
    return n;
}

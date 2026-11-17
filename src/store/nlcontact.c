/* A contact's sync identity (see nlcontact.h). */
#include "nlcontact.h"
#include "nlnumber.h"

#define NL_CONTACT_MAX_MULTI 16   /* numbers or emails hashed per card */

long nl_contact_key(const nl_vcard *v, char *out, long outsize)
{
    long n = nl_vcard_get(v, "UID", 0, out, outsize);
    if (n > 0) return n;
    n = nl_vcard_get(v, "FN", 0, out, outsize);
    if (n > 0) return n;
    if (outsize > 0) out[0] = 0;
    return NL_CONTACT_ERR_NOKEY;
}

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }

/* Append text to buf (up to cap), advancing *pos. */
static void app(char *buf, long cap, long *pos, const char *s)
{
    while (*s && *pos < cap - 1) buf[(*pos)++] = *s++;
}

/* A phone number reduced to the digits that identify it: the last nine
 * (as nl_number_same treats them), or all of a short code. Keeps two
 * writings of one number equal. */
static void number_token(const char *raw, char *out)
{
    char d[NL_NUMBER_MAX];
    int n = nl_number_digits(raw, d), start;
    if (n >= 9) start = n - 9; else start = 0;
    { int i, j = 0; for (i = start; i < n; i++) out[j++] = d[i]; out[j] = 0; }
}

static void lower(char *s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s += 32; }

/* Insertion sort of an array of fixed-width strings. */
static void sort_strs(char arr[][64], int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        char tmp[64]; int k = 0;
        while (arr[i][k]) { tmp[k] = arr[i][k]; k++; } tmp[k] = 0;
        j = i - 1;
        while (j >= 0) {
            /* strcmp(arr[j], tmp) > 0 ? */
            int a = 0, gt = 0;
            for (;; a++) {
                unsigned char x = (unsigned char)arr[j][a], y = (unsigned char)tmp[a];
                if (x != y) { gt = x > y; break; }
                if (!x) { gt = 0; break; }
            }
            if (!gt) break;
            { int m = 0; while (arr[j][m]) { arr[j+1][m] = arr[j][m]; m++; } arr[j+1][m] = 0; }
            j--;
        }
        { int m = 0; while (tmp[m]) { arr[j+1][m] = tmp[m]; m++; } arr[j+1][m] = 0; }
    }
}

nl_u32 nl_contact_hash(const nl_vcard *v)
{
    char buf[2048];
    long pos = 0;
    char val[512];
    char items[NL_CONTACT_MAX_MULTI][64];
    int i, cnt;

    /* single-valued, order-fixed fields */
    { static const char *fields[] = { "N", "FN", "ORG", "ADR", "NOTE", 0 };
      int f;
      for (f = 0; fields[f]; f++) {
          if (nl_vcard_get(v, fields[f], 0, val, sizeof val) >= 0) {
              app(buf, sizeof buf, &pos, fields[f]);
              app(buf, sizeof buf, &pos, "=");
              app(buf, sizeof buf, &pos, val);
              app(buf, sizeof buf, &pos, "\n");
          }
      }
    }

    /* numbers: normalise, sort, append */
    cnt = nl_vcard_count(v, "TEL");
    if (cnt > NL_CONTACT_MAX_MULTI) cnt = NL_CONTACT_MAX_MULTI;
    for (i = 0; i < cnt; i++) {
        if (nl_vcard_get(v, "TEL", i, val, sizeof val) >= 0) number_token(val, items[i]);
        else items[i][0] = 0;
    }
    sort_strs(items, cnt);
    for (i = 0; i < cnt; i++) { app(buf, sizeof buf, &pos, "TEL="); app(buf, sizeof buf, &pos, items[i]); app(buf, sizeof buf, &pos, "\n"); }

    /* emails: lowercase, sort, append */
    cnt = nl_vcard_count(v, "EMAIL");
    if (cnt > NL_CONTACT_MAX_MULTI) cnt = NL_CONTACT_MAX_MULTI;
    for (i = 0; i < cnt; i++) {
        if (nl_vcard_get(v, "EMAIL", i, val, sizeof val) >= 0) {
            int k = 0; while (val[k] && k < 63) { items[i][k] = val[k]; k++; } items[i][k] = 0;
            lower(items[i]);
        } else items[i][0] = 0;
    }
    sort_strs(items, cnt);
    for (i = 0; i < cnt; i++) { app(buf, sizeof buf, &pos, "EMAIL="); app(buf, sizeof buf, &pos, items[i]); app(buf, sizeof buf, &pos, "\n"); }

    (void)slen;
    return nl_crc32(buf, pos);
}

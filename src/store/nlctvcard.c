/* Build a stored vCard from a phone contact (see nlctvcard.h). */
#include <string.h>

#include "nlctvcard.h"

/* vCard TYPE word for a phone number's kind. */
static const char *tel_type(nl_num_type t)
{
    switch (t) {
    case NL_NUM_HOME:   return "HOME";
    case NL_NUM_MOBILE: return "CELL";
    case NL_NUM_FAX:    return "FAX";
    case NL_NUM_WORK:   return "WORK";
    default:            return "";      /* general: no type word */
    }
}

/* Append s to dst (bounded), returning 1 on success. */
static int cat(char *dst, long cap, const char *s)
{
    long n = (long)strlen(dst), i = 0;
    while (s[i]) { if (n + 1 >= cap) return 0; dst[n++] = s[i++]; }
    dst[n] = 0;
    return 1;
}

/* Copy s into dst, escaping ';' (so a name with ';' stays one N part). */
static void esc_semi(char *dst, long cap, const char *s)
{
    long n = 0;
    for (; *s && n + 2 < cap; s++) {
        if (*s == ';' || *s == '\\') dst[n++] = '\\';
        dst[n++] = *s;
    }
    dst[n] = 0;
}

/* YYYYMMDD into out (at least 9 bytes). */
static void datestr(nl_date d, char *out)
{
    int y = nl_date_year(d), mo = nl_date_month(d), da = nl_date_day(d);
    out[0] = (char)('0' + (y / 1000) % 10);
    out[1] = (char)('0' + (y / 100) % 10);
    out[2] = (char)('0' + (y / 10) % 10);
    out[3] = (char)('0' + y % 10);
    out[4] = (char)('0' + (mo / 10) % 10);
    out[5] = (char)('0' + mo % 10);
    out[6] = (char)('0' + (da / 10) % 10);
    out[7] = (char)('0' + da % 10);
    out[8] = 0;
}

int nl_ctvcard_build(nl_vcard *v, const nl_pbk_entry *e, const char *category,
                     nl_u8 memory, nl_date last_used, nl_date kept_on,
                     const char *uid)
{
    char buf[NL_PBK_TEXT_MAX + 16];
    char date[12];
    int k;

    nl_vcard_init(v);

    /* N:;<name>;;;  (name in the Given field, as the store's cards do) */
    buf[0] = ';';
    esc_semi(buf + 1, (long)sizeof buf - 1, e->name);
    if (!cat(buf, (long)sizeof buf, ";;;")) return NL_VC_TOO_LONG;
    if (nl_vcard_add(v, "N", 0, buf) != NL_VC_OK) return NL_VC_TOO_LONG;
    if (nl_vcard_add(v, "FN", 0, e->name) != NL_VC_OK) return NL_VC_TOO_LONG;

    for (k = 0; k < e->nnumbers; k++) {
        const char *ty = tel_type(e->numbers[k].type);
        buf[0] = 0;
        if (k == 0) { if (!cat(buf, (long)sizeof buf, "PREF")) return NL_VC_TOO_LONG; }
        if (ty[0]) {
            if (buf[0] && !cat(buf, (long)sizeof buf, ";")) return NL_VC_TOO_LONG;
            if (!cat(buf, (long)sizeof buf, ty)) return NL_VC_TOO_LONG;
        }
        if (nl_vcard_add(v, "TEL", buf[0] ? buf : 0, e->numbers[k].text) != NL_VC_OK)
            return NL_VC_TOO_LONG;
    }

    if (e->email[0] && nl_vcard_add(v, "EMAIL", "INTERNET", e->email) != NL_VC_OK)
        return NL_VC_TOO_LONG;
    if (e->note[0] && nl_vcard_add(v, "NOTE", 0, e->note) != NL_VC_OK)
        return NL_VC_TOO_LONG;

    buf[0] = 0;
    if (!cat(buf, (long)sizeof buf, "card,")) return NL_VC_TOO_LONG;
    if (!cat(buf, (long)sizeof buf, category)) return NL_VC_TOO_LONG;
    if (nl_vcard_add(v, "CATEGORIES", 0, buf) != NL_VC_OK) return NL_VC_TOO_LONG;

    if (memory == NL_PBK_MEM_ME || memory == NL_PBK_MEM_SIM) {
        if (nl_vcard_add(v, "X-NOKIA-MEMORY", 0,
                         memory == NL_PBK_MEM_SIM ? "SIM" : "PHONE") != NL_VC_OK)
            return NL_VC_TOO_LONG;
    }

    if (last_used) {
        datestr(last_used, date);
        if (nl_vcard_add(v, "X-LAST-USED", 0, date) != NL_VC_OK) return NL_VC_TOO_LONG;
    }
    /* X-KEPT-ON is always present; empty means never kept by hand. */
    if (kept_on) { datestr(kept_on, date); }
    else date[0] = 0;
    if (nl_vcard_add(v, "X-KEPT-ON", 0, date) != NL_VC_OK) return NL_VC_TOO_LONG;

    if (nl_vcard_add(v, "UID", 0, uid) != NL_VC_OK) return NL_VC_TOO_LONG;
    return NL_VC_OK;
}

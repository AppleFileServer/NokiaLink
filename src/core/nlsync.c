/* Three-way compare for sync (see nlsync.h). */
#include "nlsync.h"

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Index of key in items, or -1. */
static int find(const nl_sync_item *items, int n, const char *key)
{
    int i;
    for (i = 0; i < n; i++)
        if (streq(items[i].key, key)) return i;
    return -1;
}

nl_sync_verb nl_sync_decide(int in_base,  nl_u32 hbase,
                            int in_phone, nl_u32 hphone,
                            int in_store, nl_u32 hstore)
{
    if (in_phone && in_store) {
        if (in_base) {
            int pc = (hphone != hbase);   /* changed on phone */
            int sc = (hstore != hbase);   /* changed in store */
            if (!pc && !sc) return NL_SYNC_NONE;
            if (pc && !sc)  return NL_SYNC_UPDATE_STORE;
            if (!pc && sc)  return NL_SYNC_UPDATE_PHONE;
            return (hphone == hstore) ? NL_SYNC_NONE : NL_SYNC_CONFLICT;
        }
        /* added on both, with no base */
        return (hphone == hstore) ? NL_SYNC_NONE : NL_SYNC_CONFLICT;
    }
    if (in_phone && !in_store) {
        if (in_base) return (hphone == hbase) ? NL_SYNC_DELETE_FROM_PHONE : NL_SYNC_CONFLICT;
        return NL_SYNC_ADD_TO_STORE;
    }
    if (!in_phone && in_store) {
        if (in_base) return (hstore == hbase) ? NL_SYNC_DELETE_FROM_STORE : NL_SYNC_CONFLICT;
        return NL_SYNC_ADD_TO_PHONE;
    }
    return NL_SYNC_NONE;   /* in neither phone nor store */
}

/* Has this key already been emitted/seen? */
static int seen(const nl_sync_action *out, int n, const char *key)
{
    int i;
    for (i = 0; i < n; i++)
        if (streq(out[i].key, key)) return 1;
    return 0;
}

static int emit(nl_sync_action *out, int outmax, int *nout,
                const char *key, nl_sync_verb v)
{
    int n = *nout, i;
    if (n >= outmax) return NL_SYNC_ERR_SPACE;
    for (i = 0; i + 1 < NL_SYNC_KEY_MAX && key[i]; i++) out[n].key[i] = key[i];
    out[n].key[i] = 0;
    out[n].verb = v;
    *nout = n + 1;
    return NL_SYNC_OK;
}

/* Walk one side's keys and emit an action for each that needs one. A key
 * that needs nothing is simply not emitted; one already emitted (seen in
 * out) is skipped, so processing it again in a later walk is harmless. */
static int walk(const nl_sync_item *side, int n,
                const nl_sync_item *base,  int nbase,
                const nl_sync_item *phone, int nphone,
                const nl_sync_item *store, int nstore,
                nl_sync_action *out, int outmax, int *nout)
{
    int i;
    for (i = 0; i < n; i++) {
        const char *key = side[i].key;
        int ib, ip, is;
        nl_sync_verb v;
        if (seen(out, *nout, key)) continue;
        ib = find(base, nbase, key);
        ip = find(phone, nphone, key);
        is = find(store, nstore, key);
        v = nl_sync_decide(ib >= 0, ib >= 0 ? base[ib].hash : 0,
                           ip >= 0, ip >= 0 ? phone[ip].hash : 0,
                           is >= 0, is >= 0 ? store[is].hash : 0);
        if (v != NL_SYNC_NONE) {
            int r = emit(out, outmax, nout, key, v);
            if (r != NL_SYNC_OK) return r;
        }
    }
    return NL_SYNC_OK;
}

int nl_sync_compare(const nl_sync_item *base,  int nbase,
                    const nl_sync_item *phone, int nphone,
                    const nl_sync_item *store, int nstore,
                    nl_sync_action *out, int outmax, int *nout)
{
    int r;
    *nout = 0;
    r = walk(phone, nphone, base, nbase, phone, nphone, store, nstore, out, outmax, nout);
    if (r != NL_SYNC_OK) return r;
    r = walk(store, nstore, base, nbase, phone, nphone, store, nstore, out, outmax, nout);
    if (r != NL_SYNC_OK) return r;
    r = walk(base, nbase, base, nbase, phone, nphone, store, nstore, out, outmax, nout);
    return r;
}

const char *nl_sync_verb_name(nl_sync_verb v)
{
    switch (v) {
    case NL_SYNC_ADD_TO_STORE:      return "add to store";
    case NL_SYNC_ADD_TO_PHONE:      return "add to phone";
    case NL_SYNC_UPDATE_STORE:      return "update store";
    case NL_SYNC_UPDATE_PHONE:      return "update phone";
    case NL_SYNC_DELETE_FROM_PHONE: return "delete from phone";
    case NL_SYNC_DELETE_FROM_STORE: return "delete from store";
    case NL_SYNC_CONFLICT:          return "conflict";
    default:                        return "none";
    }
}

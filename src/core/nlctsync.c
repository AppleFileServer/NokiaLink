/* Contacts sync orchestration (see nlctsync.h). */
#include "nlctsync.h"

static void copykey(char *dst, const char *src)
{ int i = 0; while (i + 1 < NL_SYNC_KEY_MAX && src[i]) { dst[i] = src[i]; i++; } dst[i] = 0; }

nl_ct_op nl_ct_policy(nl_sync_verb v)
{
    switch (v) {
    case NL_SYNC_ADD_TO_STORE:      return NL_CT_ADD_TO_STORE;
    case NL_SYNC_ADD_TO_PHONE:      return NL_CT_ADD_TO_PHONE;
    case NL_SYNC_UPDATE_STORE:      return NL_CT_UPDATE_STORE;
    case NL_SYNC_UPDATE_PHONE:      return NL_CT_UPDATE_PHONE;
    case NL_SYNC_DELETE_FROM_PHONE: return NL_CT_REMOVE_FROM_PHONE;  /* gone from store Main */
    case NL_SYNC_DELETE_FROM_STORE: return NL_CT_MOVE_TO_ARCHIVE;    /* gone from phone (dec.10) */
    default:                        return NL_CT_CONFLICT;
    }
}

int nl_ct_map(const nl_sync_action *in, int nin,
              nl_ct_archive_match match, void *ctx,
              nl_ct_action *out, int outmax, int *nout)
{
    int i, n = 0;
    for (i = 0; i < nin; i++) {
        nl_ct_op op = nl_ct_policy(in[i].verb);
        char archived[NL_SYNC_KEY_MAX];
        if (n >= outmax) { *nout = n; return NL_CT_ERR_SPACE; }

        /* Decision 11: a new phone contact whose number matches an
         * Archived contact revives that contact rather than adding a new one. */
        if (op == NL_CT_ADD_TO_STORE && match &&
            match(ctx, in[i].key, archived, (long)sizeof archived)) {
            out[n].op = NL_CT_UNARCHIVE;
            copykey(out[n].key, in[i].key);
            copykey(out[n].other, archived);
        } else {
            out[n].op = op;
            copykey(out[n].key, in[i].key);
            out[n].other[0] = 0;
        }
        n++;
    }
    *nout = n;
    return NL_CT_OK;
}

const char *nl_ct_op_name(nl_ct_op op)
{
    switch (op) {
    case NL_CT_ADD_TO_STORE:      return "add to store";
    case NL_CT_ADD_TO_PHONE:      return "add to phone";
    case NL_CT_UPDATE_STORE:      return "update store";
    case NL_CT_UPDATE_PHONE:      return "update phone";
    case NL_CT_REMOVE_FROM_PHONE: return "remove from phone";
    case NL_CT_MOVE_TO_ARCHIVE:   return "move to archive";
    case NL_CT_UNARCHIVE:         return "un-archive";
    case NL_CT_CONFLICT:          return "conflict";
    default:                      return "?";
    }
}

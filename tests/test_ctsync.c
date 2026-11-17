/* Host tests for the contacts sync orchestration (src/core/nlctsync).
 * Run by tests/run-host.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlctsync.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

/* match stub: the phone contact "revive-me" matches archived "arch-7". */
static int match_stub(void *ctx, const char *phone_key, char *out, long outsize)
{
    (void)ctx; (void)outsize;
    if (strcmp(phone_key, "revive-me") == 0) { strcpy(out, "arch-7"); return 1; }
    return 0;
}

static void test_policy(void)
{
    current = "policy mapping";
    CHECK(nl_ct_policy(NL_SYNC_ADD_TO_STORE)      == NL_CT_ADD_TO_STORE);
    CHECK(nl_ct_policy(NL_SYNC_ADD_TO_PHONE)      == NL_CT_ADD_TO_PHONE);
    CHECK(nl_ct_policy(NL_SYNC_UPDATE_STORE)      == NL_CT_UPDATE_STORE);
    CHECK(nl_ct_policy(NL_SYNC_UPDATE_PHONE)      == NL_CT_UPDATE_PHONE);
    CHECK(nl_ct_policy(NL_SYNC_DELETE_FROM_PHONE) == NL_CT_REMOVE_FROM_PHONE);
    CHECK(nl_ct_policy(NL_SYNC_DELETE_FROM_STORE) == NL_CT_MOVE_TO_ARCHIVE);
    CHECK(nl_ct_policy(NL_SYNC_CONFLICT)          == NL_CT_CONFLICT);
}

static nl_ct_op op_of(const nl_ct_action *a, int n, const char *key)
{
    int i; for (i = 0; i < n; i++) if (strcmp(a[i].key, key) == 0) return a[i].op;
    return NL_CT_CONFLICT;
}

static void test_map(void)
{
    nl_sync_action in[] = {
        { "new-plain",  NL_SYNC_ADD_TO_STORE },      /* new on phone, no archive match */
        { "revive-me",  NL_SYNC_ADD_TO_STORE },      /* new on phone, matches archive */
        { "from-store", NL_SYNC_ADD_TO_PHONE },
        { "chg-phone",  NL_SYNC_UPDATE_STORE },
        { "gone-store", NL_SYNC_DELETE_FROM_PHONE },
        { "gone-phone", NL_SYNC_DELETE_FROM_STORE }, /* -> move to archive */
        { "both-chg",   NL_SYNC_CONFLICT }
    };
    nl_ct_action out[16]; int n;
    current = "map with archive match";
    CHECK(nl_ct_map(in, 7, match_stub, 0, out, 16, &n) == NL_CT_OK && n == 7);
    CHECK(op_of(out,n,"new-plain")  == NL_CT_ADD_TO_STORE);
    CHECK(op_of(out,n,"from-store") == NL_CT_ADD_TO_PHONE);
    CHECK(op_of(out,n,"chg-phone")  == NL_CT_UPDATE_STORE);
    CHECK(op_of(out,n,"gone-store") == NL_CT_REMOVE_FROM_PHONE);
    CHECK(op_of(out,n,"gone-phone") == NL_CT_MOVE_TO_ARCHIVE);
    CHECK(op_of(out,n,"both-chg")   == NL_CT_CONFLICT);
    /* the revive case became UNARCHIVE with the archived key */
    { int i, found=0; for(i=0;i<n;i++) if(strcmp(out[i].key,"revive-me")==0){
        found=1; CHECK(out[i].op==NL_CT_UNARCHIVE && strcmp(out[i].other,"arch-7")==0); }
      CHECK(found); }

    current = "map without match callback: plain add";
    CHECK(nl_ct_map(in, 2, 0, 0, out, 16, &n) == NL_CT_OK);
    CHECK(op_of(out,n,"revive-me") == NL_CT_ADD_TO_STORE);   /* no revive without cb */

    current = "map out of space";
    CHECK(nl_ct_map(in, 7, 0, 0, out, 3, &n) == NL_CT_ERR_SPACE);
}

/* full pipeline: compare -> map */
static void test_pipeline(void)
{
    nl_sync_item base[]  = { {"a",1}, {"b",1} };
    nl_sync_item phone[] = { {"a",1} };            /* b gone from phone -> archive */
    nl_sync_item store[] = { {"a",1}, {"b",1}, {"c",1} };  /* c new in store -> to phone */
    nl_sync_action sa[8]; int nsa;
    nl_ct_action plan[8]; int np;
    current = "pipeline compare->map";
    CHECK(nl_sync_compare(base,2, phone,1, store,3, sa, 8, &nsa) == NL_SYNC_OK);
    CHECK(nl_ct_map(sa, nsa, 0, 0, plan, 8, &np) == NL_CT_OK);
    CHECK(op_of(plan,np,"b") == NL_CT_MOVE_TO_ARCHIVE);
    CHECK(op_of(plan,np,"c") == NL_CT_ADD_TO_PHONE);
}

int main(void)
{
    test_policy();
    test_map();
    test_pipeline();
    printf("ctsync: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

/* Host tests for the three-way sync compare (src/core/nlsync).
 * Run by tests/run-host.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/core/nlsync.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

#define A 1u
#define B 2u
#define C 3u

static void test_decide(void)
{
    current = "decide";
    CHECK(nl_sync_decide(1,A, 1,A, 1,A) == NL_SYNC_NONE);          /* unchanged */
    CHECK(nl_sync_decide(1,A, 1,B, 1,A) == NL_SYNC_UPDATE_STORE);  /* phone changed */
    CHECK(nl_sync_decide(1,A, 1,A, 1,B) == NL_SYNC_UPDATE_PHONE);  /* store changed */
    CHECK(nl_sync_decide(1,A, 1,B, 1,B) == NL_SYNC_NONE);          /* same change both */
    CHECK(nl_sync_decide(1,A, 1,B, 1,C) == NL_SYNC_CONFLICT);      /* diff change both */
    CHECK(nl_sync_decide(0,0, 1,A, 1,A) == NL_SYNC_NONE);          /* added both, same */
    CHECK(nl_sync_decide(0,0, 1,A, 1,B) == NL_SYNC_CONFLICT);      /* added both, diff */
    CHECK(nl_sync_decide(1,A, 1,A, 0,0) == NL_SYNC_DELETE_FROM_PHONE); /* store deleted */
    CHECK(nl_sync_decide(1,A, 1,B, 0,0) == NL_SYNC_CONFLICT);      /* store del, phone chg */
    CHECK(nl_sync_decide(0,0, 1,A, 0,0) == NL_SYNC_ADD_TO_STORE);  /* new on phone */
    CHECK(nl_sync_decide(1,A, 0,0, 1,A) == NL_SYNC_DELETE_FROM_STORE); /* phone deleted */
    CHECK(nl_sync_decide(1,A, 0,0, 1,B) == NL_SYNC_CONFLICT);      /* phone del, store chg */
    CHECK(nl_sync_decide(0,0, 0,0, 1,A) == NL_SYNC_ADD_TO_PHONE);  /* new on store */
    CHECK(nl_sync_decide(1,A, 0,0, 0,0) == NL_SYNC_NONE);          /* deleted both */
}

static nl_sync_verb verb_of(const nl_sync_action *a, int n, const char *key)
{
    int i;
    for (i = 0; i < n; i++) if (strcmp(a[i].key, key) == 0) return a[i].verb;
    return NL_SYNC_NONE;
}

static void test_compare(void)
{
    nl_sync_item base[]  = { {"k1",A}, {"k2",A}, {"k3",A}, {"k4",A} };
    nl_sync_item phone[] = { {"k1",A}, {"k2",B}, {"k4",A}, {"k5",A} };
    nl_sync_item store[] = { {"k1",A}, {"k2",A}, {"k3",A}, {"k6",A} };
    nl_sync_action out[16]; int n, r;
    current = "compare mix";
    r = nl_sync_compare(base,4, phone,4, store,4, out, 16, &n);
    CHECK(r == NL_SYNC_OK);
    CHECK(n == 5);                                   /* k1 unchanged -> no action */
    CHECK(verb_of(out,n,"k1") == NL_SYNC_NONE);
    CHECK(verb_of(out,n,"k2") == NL_SYNC_UPDATE_STORE);
    CHECK(verb_of(out,n,"k3") == NL_SYNC_DELETE_FROM_STORE);  /* gone from phone */
    CHECK(verb_of(out,n,"k4") == NL_SYNC_DELETE_FROM_PHONE);  /* gone from store */
    CHECK(verb_of(out,n,"k5") == NL_SYNC_ADD_TO_STORE);       /* new on phone */
    CHECK(verb_of(out,n,"k6") == NL_SYNC_ADD_TO_PHONE);       /* new in store */
    /* no key appears twice */
    { int i,j,dup=0; for(i=0;i<n;i++) for(j=i+1;j<n;j++) if(!strcmp(out[i].key,out[j].key)) dup=1;
      CHECK(!dup); }
}

static void test_conflict_and_empty(void)
{
    nl_sync_item base[]  = { {"c",A} };
    nl_sync_item phone[] = { {"c",B} };
    nl_sync_item store[] = { {"c",C} };
    nl_sync_action out[4]; int n;
    current = "compare conflict";
    CHECK(nl_sync_compare(base,1, phone,1, store,1, out,4,&n) == NL_SYNC_OK);
    CHECK(n==1 && out[0].verb==NL_SYNC_CONFLICT && strcmp(out[0].key,"c")==0);

    current = "all identical -> nothing";
    { nl_sync_item s[] = {{"a",A},{"b",B}};
      CHECK(nl_sync_compare(s,2, s,2, s,2, out,4,&n)==NL_SYNC_OK && n==0); }

    current = "first sync: empty base and store, phone has items";
    { nl_sync_item ph[] = {{"x",A},{"y",B}};
      CHECK(nl_sync_compare(0,0, ph,2, 0,0, out,4,&n)==NL_SYNC_OK && n==2);
      CHECK(verb_of(out,n,"x")==NL_SYNC_ADD_TO_STORE && verb_of(out,n,"y")==NL_SYNC_ADD_TO_STORE); }
}

static void test_space(void)
{
    nl_sync_item phone[] = { {"a",A},{"b",A},{"c",A} };
    nl_sync_action out[2]; int n;
    current = "out of space";
    CHECK(nl_sync_compare(0,0, phone,3, 0,0, out,2,&n) == NL_SYNC_ERR_SPACE);
}

int main(void)
{
    test_decide();
    test_compare();
    test_conflict_and_empty();
    test_space();
    printf("sync: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

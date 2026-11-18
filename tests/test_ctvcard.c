/* Host tests for building a stored vCard from a phone contact
 * (src/store/nlctvcard). Run by tests/run-host.sh. Data is made up. */
#include <stdio.h>
#include <string.h>
#include "../src/store/nlctvcard.h"

static int checks, failures;
static const char *current;
#define CHECK(c) do { checks++; if (!(c)) { failures++; \
    printf("FAIL %s: line %d: %s\n", current, __LINE__, #c); } } while (0)

static long getv(const nl_vcard *v, const char *name, char *out)
{ return nl_vcard_get(v, name, 0, out, 256); }

static void test_build(void)
{
    nl_pbk_entry e;
    nl_vcard v;
    char out[256];

    memset(&e, 0, sizeof e);
    strcpy(e.name, "Ada Lovelace");
    e.nnumbers = 2;
    e.numbers[0].type = NL_NUM_MOBILE; strcpy(e.numbers[0].text, "+447700900001");
    e.numbers[1].type = NL_NUM_HOME;   strcpy(e.numbers[1].text, "01772555000");
    strcpy(e.email, "ada@example.org");

    current = "build ok";
    CHECK(nl_ctvcard_build(&v, &e, "Main", NL_PBK_MEM_SIM,
                           nl_date_make(2026,11,16), 0, "nokialink-abc123") == NL_VC_OK);

    current = "fields";
    CHECK(getv(&v,"FN",out) > 0 && strcmp(out,"Ada Lovelace")==0);
    CHECK(getv(&v,"N",out) > 0 && strcmp(out,";Ada Lovelace;;;")==0);
    CHECK(getv(&v,"CATEGORIES",out) > 0 && strcmp(out,"card,Main")==0);
    CHECK(getv(&v,"X-NOKIA-MEMORY",out) > 0 && strcmp(out,"SIM")==0);
    CHECK(getv(&v,"X-LAST-USED",out) > 0 && strcmp(out,"20261116")==0);
    CHECK(getv(&v,"UID",out) > 0 && strcmp(out,"nokialink-abc123")==0);
    CHECK(getv(&v,"EMAIL",out) > 0 && strcmp(out,"ada@example.org")==0);

    current = "two TEL";
    CHECK(nl_vcard_count(&v,"TEL")==2);
    CHECK(nl_vcard_get(&v,"TEL",0,out,sizeof out) > 0 && strcmp(out,"+447700900001")==0);
    CHECK(nl_vcard_get(&v,"TEL",1,out,sizeof out) > 0 && strcmp(out,"01772555000")==0);
    current = "TEL params";
    CHECK(nl_vcard_params(&v,"TEL",0,out,sizeof out) > 0 && nl_vcard_has_param(out,"PREF")
          && nl_vcard_has_param(out,"CELL"));
    CHECK(nl_vcard_params(&v,"TEL",1,out,sizeof out) > 0 && nl_vcard_has_param(out,"HOME"));

    current = "kept-on empty";
    CHECK(nl_vcard_get(&v,"X-KEPT-ON",0,out,sizeof out) == 0);   /* present, empty */
}

static void test_reload(void)
{
    nl_pbk_entry e;
    nl_vcard v, v2;
    char out[256];
    memset(&e, 0, sizeof e);
    strcpy(e.name, "Grace");
    e.nnumbers = 1; e.numbers[0].type = NL_NUM_GENERAL; strcpy(e.numbers[0].text, "07700900003");
    nl_ctvcard_build(&v, &e, "Archive", NL_PBK_MEM_ME, 0, 0, "nokialink-grace");

    current = "reloads as a card";
    CHECK(nl_vcard_load(&v2, v.text, v.len) == NL_VC_OK);
    CHECK(getv(&v2,"FN",out) > 0 && strcmp(out,"Grace")==0);
    CHECK(getv(&v2,"CATEGORIES",out) > 0 && strcmp(out,"card,Archive")==0);
    CHECK(getv(&v2,"X-NOKIA-MEMORY",out) > 0 && strcmp(out,"PHONE")==0);
    current = "no last-used line when 0";
    CHECK(nl_vcard_get(&v2,"X-LAST-USED",0,out,sizeof out) == NL_VC_MISSING);
}

int main(void)
{
    test_build();
    test_reload();
    printf("ctvcard: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}

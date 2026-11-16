/* Telephone number matching (nlnumber.h). */
#include "nlnumber.h"

#define SIGNIFICANT 9           /* digits compared, counted from the end */
#define SHORTEST    7           /* fewer digits than this: a short code */

int nl_number_digits(const char *written, char *out)
{
    int n = 0;
    if (written)
        for (; *written; written++)
            if (*written >= '0' && *written <= '9' && n < NL_NUMBER_MAX - 1)
                out[n++] = *written;
    out[n] = 0;
    return n;
}

int nl_number_same(const char *a, const char *b)
{
    char da[NL_NUMBER_MAX], db[NL_NUMBER_MAX];
    int la = nl_number_digits(a, da), lb = nl_number_digits(b, db);
    int n, i;

    if (la == 0 || lb == 0) return 0;
    if (la < SHORTEST || lb < SHORTEST) {
        if (la != lb) return 0;
        n = la;
    } else {
        n = la < lb ? la : lb;
        if (n > SIGNIFICANT) n = SIGNIFICANT;
    }
    for (i = 1; i <= n; i++)
        if (da[la - i] != db[lb - i]) return 0;
    return 1;
}

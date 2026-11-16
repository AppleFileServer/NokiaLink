/* Telling whether two telephone numbers are the same number.
 *
 * Portable C, no allocation. Used to match the numbers in texts and
 * call records against contacts (SPECIFICATION.md USE-2), so that
 * "07700 900123", "+447700900123" and "00447700900123" all count as one.
 */
#ifndef NL_NUMBER_H
#define NL_NUMBER_H

#define NL_NUMBER_MAX 24        /* digits kept from a number, with the zero byte */

/* Copies the digits of a number as written (spaces, brackets, dashes and
 * a leading "+" dropped) into out, at most NL_NUMBER_MAX - 1 of them.
 * Returns how many digits there are. */
int nl_number_digits(const char *written, char *out);

/* 1 if the two are the same number, 0 if not.
 *  - Seven digits or more each: the same if the last nine digits agree
 *    (or all of the shorter, if it has fewer than nine), which makes the
 *    national and international forms equal.
 *  - Shorter numbers (short codes such as 38885): only if every digit
 *    agrees.
 * An empty number matches nothing. */
int nl_number_same(const char *a, const char *b);

#endif

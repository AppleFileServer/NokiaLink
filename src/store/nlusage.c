/* The 18-month check and the clock rules (nlusage.h). */
#include "nlusage.h"

int nl_check_due(nl_date last_check, nl_date today)
{
    if (!nl_date_valid(today)) return 0;
    if (!nl_date_valid(last_check)) return 1;
    return today / 100 > last_check / 100;      /* a later year-and-month */
}

int nl_contact_unused(nl_date last_used, nl_date kept_on, nl_date today)
{
    nl_date latest = last_used > kept_on ? last_used : kept_on;
    nl_date limit = nl_date_add_months(today, -NL_UNUSED_MONTHS);
    if (limit == 0) return 0;                   /* no usable "today" */
    if (!nl_date_valid(latest)) return 1;
    return latest < limit;
}

nl_date nl_last_used_after(nl_date last_used, nl_date seen, nl_date today)
{
    if (!nl_date_valid(seen) || seen > today) return last_used;
    return seen > last_used ? seen : last_used;
}

nl_date nl_last_used_when_added(nl_date arrived_with, nl_date today)
{
    if (nl_date_valid(arrived_with) && arrived_with <= today) return arrived_with;
    return nl_date_valid(today) ? today : 0;
}

nl_stamp nl_clock_floor(nl_stamp last_sync, nl_stamp newest_text)
{
    return nl_stamp_cmp(last_sync, newest_text) >= 0 ? last_sync : newest_text;
}

int nl_clock_trusted(nl_stamp plus, nl_stamp floor)
{
    if (!nl_date_valid(plus.date)) return 0;
    if (nl_date_year(plus.date) < NL_CLOCK_FIRST_YEAR) return 0;
    return nl_stamp_cmp(plus, floor) >= 0;
}

nl_clock_action nl_clock_decide(nl_stamp plus, nl_stamp phone, int phone_set,
                                nl_stamp floor, int first_connection)
{
    long apart;

    if (first_connection) return NL_CLOCK_CONFIRM_FIRST;
    if (!nl_clock_trusted(plus, floor)) return NL_CLOCK_PLUS_WRONG;
    if (!phone_set || !nl_date_valid(phone.date)) return NL_CLOCK_ASK;

    apart = nl_stamp_diff(plus, phone);
    if (apart < 0) apart = -apart;
    if (apart <= NL_CLOCK_CLOSE_SECS) return NL_CLOCK_SET_PHONE;
    if (apart >= 3600L - NL_CLOCK_HOUR_SLACK && apart <= 3600L + NL_CLOCK_HOUR_SLACK)
        return NL_CLOCK_ASK_HOUR;
    return NL_CLOCK_ASK;
}

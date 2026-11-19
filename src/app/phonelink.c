/* The live link to the phone (see phonelink.h): nl_link over the SCC port. */
#include <string.h>

#include "phonelink.h"
#include "scc.h"

static nl_port gPort;
static nl_link gLink;
static nl_u8   gMsg[1024];   /* the longest message the link will take in */
static int     gOpen;

int phonelink_open(int *at_ok)
{
    int rc;

    if (at_ok) *at_ok = 0;
    if (gOpen) phonelink_close();

    memset(&gPort, 0, sizeof gPort);
    if (nl_scc_open(&gPort) != 0)
        return NL_ERR_IO;

    nl_link_init(&gLink, &gPort, gMsg, sizeof gMsg, 0, 0);
    rc = nl_link_start_dlr3(&gLink);
    if (at_ok) *at_ok = gLink.at_ok;
    if (rc != NL_OK) {
        nl_scc_close();
        return rc;
    }
    gOpen = 1;
    return NL_OK;
}

int phonelink_is_open(void)
{
    return gOpen;
}

int phonelink_request(nl_u8 type, const nl_u8 *req, long reqlen,
                      nl_u8 reply_type, nl_u8 *reply, long reply_cap,
                      long *reply_len, nl_u32 timeout_ms)
{
    if (!gOpen) return NL_ERR_IO;
    return nl_link_request(&gLink, type, req, reqlen, reply_type,
                           reply, reply_cap, reply_len, timeout_ms);
}

int phonelink_send(nl_u8 type, const nl_u8 *data, long len)
{
    if (!gOpen) return NL_ERR_IO;
    return nl_link_send(&gLink, type, data, len);
}

void phonelink_poll(void)
{
    if (gOpen) nl_link_poll(&gLink);
}

void phonelink_close(void)
{
    if (!gOpen) return;
    nl_scc_close();
    gOpen = 0;
}

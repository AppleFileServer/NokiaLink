/* The live link to the phone, for the Plus application: one FBUS session
 * over the SCC (modem port), opened once and shared by every view and
 * action. It wraps the portable nl_link (src/core) over the Plus's nl_scc
 * port (src/plus), so main.c and the dialogs never touch the serial chip
 * or the framing directly.
 *
 * 68k Macintosh only (the port is the SCC). UNTESTED ON HARDWARE until the
 * DLR-3P adapter lead exists; see src/plus/scc.h and docs/PLAN.md section 2.
 */
#ifndef NL_PHONELINK_H
#define NL_PHONELINK_H

#include "../core/link.h"

/* Open the link: set up the SCC port and start an FBUS session
 * (nl_link_start_dlr3). Returns NL_OK if the session started, or a negative
 * NL_ERR_*. *at_ok (may be 0) receives how many of the three start-up
 * commands the phone answered "OK" -- a quick sign the cable is right. */
int  phonelink_open(int *at_ok);

/* Is the link open? */
int  phonelink_is_open(void);

/* Send a request and wait for the matching reply (see nl_link_request).
 * Returns NL_ERR_IO if the link is not open. */
int  phonelink_request(nl_u8 type, const nl_u8 *req, long reqlen,
                       nl_u8 reply_type, nl_u8 *reply, long reply_cap,
                       long *reply_len, nl_u32 timeout_ms);

/* Send a message with acknowledgement, expecting no reply. */
int  phonelink_send(nl_u8 type, const nl_u8 *data, long len);

/* Take in whatever has arrived; call it while idle. */
void phonelink_poll(void);

/* Close the link and return the SCC to a quiet state. */
void phonelink_close(void);

#endif

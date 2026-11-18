/* The Macintosh Plus serial link: an nl_port that drives channel A of the
 * SCC (Zilog Z8530) on the modem port directly, because the ROM Serial
 * Driver stops at 57,600 baud and the phone needs 115,200 (docs/PLAN.md
 * section 2.1, docs/CABLE.md).
 *
 * Channel A = the modem port (the printer port, channel B, is left for
 * LocalTalk). Start-up runs at 19,125 baud (the chip's nearest to 19,200,
 * for the AT commands), then 114,750 baud for FBUS: the chip's 3.672 MHz
 * clock divided by 32.
 *
 * This first version is POLLED: write waits for the transmit buffer, read
 * takes whatever has already arrived without waiting (as nl_port asks). At
 * 114,750 baud polling may miss bytes on an 8 MHz Plus; if it does, the
 * follow-up is an interrupt routine that fills a ring buffer (PLAN.md).
 *
 * The mouse shares the SCC, so this touches only channel A's own
 * registers and never resets the whole chip; nl_scc_close puts channel A
 * back to a quiet state.
 *
 * UNTESTED ON HARDWARE: written from the Z8530 manuals and Inside
 * Macintosh; it needs a real Plus and the DLR-3P adapter lead to verify.
 *
 * 68k Macintosh only (uses the SCC and the Toolbox); not built on the host.
 */
#ifndef NL_SCC_H
#define NL_SCC_H

#include "../core/link.h"

/* Set up *port to drive SCC channel A (modem port), polled, 8N1, DTR off.
 * Returns 0, or -1 if the SCC base could not be found. */
int  nl_scc_open(nl_port *port);

/* Return channel A to a quiet state (receiver and transmitter off, DTR
 * low). Does not disturb channel B or the mouse. */
void nl_scc_close(void);

#endif

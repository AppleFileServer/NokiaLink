/* Macintosh Plus SCC channel-A serial link (see scc.h).
 *
 * UNTESTED ON HARDWARE. Written from:
 *   - Zilog Z8530 SCC Technical Manual / User's Manual (register layout),
 *   - Inside Macintosh / Guide to Macintosh Family Hardware (SCC addresses,
 *     the 3.672 MHz clock, the low-memory base pointers),
 *   - docs/PLAN.md section 2.1 and docs/CABLE.md.
 *
 * Mouse coexistence: on the Plus the mouse shares the SCC (channel A's
 * external/status line reports one axis). So this driver, while polled,
 * deliberately NEVER issues a channel reset and NEVER writes the
 * interrupt registers (WR1, WR9, WR15). It writes only the data-format,
 * clock/baud and enable registers (WR3, WR4, WR5, WR11-WR14), which the
 * mouse does not depend on. This is the key thing to confirm on hardware.
 */
#include "scc.h"

/* ---- SCC addresses via the documented low-memory base pointers -------
 * SCCRd (0x0D8... at 0x1D8) points at the read base, SCCWr (0x1DC) at the
 * write base. Channel B control is +0, channel A control +2, channel B
 * data +4, channel A data +6. Access is byte-wide. */
#define SCC_RD_BASE (*(volatile unsigned char **)0x1D8L)
#define SCC_WR_BASE (*(volatile unsigned char **)0x1DCL)

static volatile unsigned char *aCtlR, *aCtlW, *aDataR, *aDataW;

/* The SCC needs a short recovery time between accesses. A few dummy reads
 * are plenty at 8 MHz; kept deliberately conservative. */
static void scc_delay(void)
{
    volatile short i;
    for (i = 0; i < 6; i++) { /* spin */ }
}

/* Write val to write-register reg of channel A (reg 0 is direct). */
static void scc_wr(int reg, unsigned char val)
{
    if (reg != 0) { *aCtlW = (unsigned char)reg; scc_delay(); }
    *aCtlW = val; scc_delay();
}

/* Read RR0 (status) of channel A. */
static unsigned char scc_rr0(void)
{
    unsigned char v = *aCtlR; scc_delay();
    return v;
}

/* RR0 bits we use. */
#define RR0_RX_AVAIL  0x01   /* a received character is ready */
#define RR0_TX_EMPTY  0x04   /* the transmit buffer is empty */

/* ---- baud ------------------------------------------------------------
 * The Plus feeds the SCC a 3.672 MHz clock. Two cases:
 *   19,200 -> 19,125: BRG from PCLK, x16 clock, time constant 4
 *             (3,672,000 / (2*(4+2)*16) = 19,125).
 *   115,200 -> 114,750: x32 clock taken straight from the RTxC pin, BRG
 *             off (3,672,000 / 32 = 114,750).
 * Other speeds use the BRG with the nearest time constant at x16. */
static int scc_set_baud(long baud)
{
    if (baud >= 100000L) {
        /* high speed: x32, clock from RTxC, BRG disabled */
        scc_wr(14, 0x00);          /* WR14: BRG off */
        scc_wr(4,  0x84);          /* WR4: x32 clock, 1 stop, no parity */
        scc_wr(11, 0x28);          /* WR11: Rx clk = RTxC, Tx clk = RTxC */
        return 0;
    } else {
        /* BRG at x16 from PCLK; time constant = clock/(2*16*baud) - 2 */
        long tc = (3672000L / (2L * 16L * baud)) - 2L;
        if (tc < 0) tc = 0;
        if (tc > 0xFFFF) tc = 0xFFFF;
        scc_wr(4,  0x44);                       /* WR4: x16 clock, 1 stop, no parity */
        scc_wr(11, 0x50);                       /* WR11: Rx & Tx clock from BRG */
        scc_wr(12, (unsigned char)(tc & 0xFF)); /* WR12: TC low */
        scc_wr(13, (unsigned char)((tc >> 8) & 0xFF)); /* WR13: TC high */
        scc_wr(14, 0x03);                       /* WR14: BRG source PCLK, BRG enable */
        return 0;
    }
}

/* ---- nl_port callbacks ----------------------------------------------- */

static unsigned char g_wr5;   /* shadow of WR5 so set_power keeps Tx bits */

static long scc_write(void *ctx, const nl_u8 *bytes, long n)
{
    long i;
    (void)ctx;
    for (i = 0; i < n; i++) {
        /* wait (bounded) for the transmit buffer to empty */
        long guard = 200000L;
        while (!(scc_rr0() & RR0_TX_EMPTY) && guard-- > 0) { /* spin */ }
        *aDataW = bytes[i]; scc_delay();
    }
    return n;
}

static long scc_read(void *ctx, nl_u8 *bytes, long max)
{
    long n = 0;
    (void)ctx;
    while (n < max && (scc_rr0() & RR0_RX_AVAIL)) {
        bytes[n++] = *aDataR; scc_delay();
    }
    return n;
}

static int scc_set_speed(void *ctx, long baud)
{
    (void)ctx;
    return scc_set_baud(baud);
}

static int scc_set_power(void *ctx, int on)
{
    (void)ctx;
    /* DTR (pin HSKo) powers the DLR-3P cable's converter (CABLE.md). */
    if (on) g_wr5 |= 0x80; else g_wr5 &= (unsigned char)~0x80;
    scc_wr(5, g_wr5);
    return 0;
}

/* The Ticks low-memory global counts ~60.15 Hz; convert to milliseconds
 * (50/3 per tick), split to stay clear of 32-bit overflow. */
static nl_u32 scc_now_ms(void *ctx)
{
    unsigned long t = *(volatile unsigned long *)0x16AL;
    (void)ctx;
    return (nl_u32)((t / 3UL) * 50UL + ((t % 3UL) * 50UL) / 3UL);
}

static void scc_sleep_ms(void *ctx, nl_u32 ms)
{
    nl_u32 start = scc_now_ms(ctx);
    while ((nl_u32)(scc_now_ms(ctx) - start) < ms) { /* spin */ }
}

int nl_scc_open(nl_port *port)
{
    if (SCC_RD_BASE == 0 || SCC_WR_BASE == 0) return -1;
    aCtlR  = SCC_RD_BASE + 2; aDataR = SCC_RD_BASE + 6;
    aCtlW  = SCC_WR_BASE + 2; aDataW = SCC_WR_BASE + 6;

    /* Data format: 8 bits, receiver and transmitter on. DTR off until the
     * caller powers the cable. No interrupt registers are touched. */
    scc_wr(3, 0xC1);           /* WR3: Rx 8 bits/char, Rx enable */
    g_wr5 = 0x68;              /* WR5: Tx 8 bits/char, Tx enable, DTR off */
    scc_wr(5, g_wr5);
    scc_set_baud(19200L);      /* start-up speed (becomes 19,125) */

    port->ctx       = 0;
    port->write     = scc_write;
    port->read      = scc_read;
    port->set_speed = scc_set_speed;
    port->set_power = scc_set_power;
    port->now_ms    = scc_now_ms;
    port->sleep_ms  = scc_sleep_ms;
    return 0;
}

void nl_scc_close(void)
{
    /* Quiet channel A: receiver and transmitter off, DTR low. Interrupt
     * registers are left as they were, so the mouse keeps working. */
    scc_wr(3, 0xC0);           /* Rx disable (keep 8-bit setting) */
    g_wr5 = 0x60;              /* Tx disable, DTR low */
    scc_wr(5, g_wr5);
}

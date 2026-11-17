/* NokiaLink's emulator smoke test: runs the portable modules' key checks
 * on the real 68000 under Mini vMac, to prove the Retro68 build computes
 * the same as the host tests (byte order, 32-bit long, and so on). Built
 * into the application only when NOKIALINK_SMOKE is defined. */
#ifndef NL_SMOKE_H
#define NL_SMOKE_H

void Smoke_Run(void);   /* run the checks, write results to the file "out", quit */

#endif

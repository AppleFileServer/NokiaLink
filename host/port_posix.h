/* An nl_port over a POSIX serial device (a USB-to-serial adapter on the
 * modern Mac), for the host tool. Not part of the Plus application. */
#ifndef PORT_POSIX_H
#define PORT_POSIX_H

#include <stdio.h>
#include "../src/core/link.h"

typedef struct {
    nl_port port;
    int fd;
    FILE *trace;            /* if set, every byte each way is logged here */
    int trace_dir;          /* private: direction of the line being written */
    int trace_col;
} posix_port;

/* Opens the device (for example /dev/cu.usbserial-XXXX) at 19,200 baud,
 * 8 data bits, no parity, no flow control. Returns 0, or -1 with errno set. */
int posix_port_open(posix_port *pp, const char *device);
void posix_port_close(posix_port *pp);

#endif

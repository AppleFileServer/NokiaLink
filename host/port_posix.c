/* An nl_port over a POSIX serial device. See port_posix.h. */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "port_posix.h"

static nl_u32 pp_now(void *ctx)
{
    struct timespec ts;
    (void)ctx;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (nl_u32)ts.tv_sec * 1000UL + (nl_u32)(ts.tv_nsec / 1000000L);
}

static void pp_sleep(void *ctx, nl_u32 ms)
{
    struct timespec ts;
    (void)ctx;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, 0);
}

/* Trace lines look like:  ">   1234 1E 00 0C D1 ..."  (> out, < in). */
static void trace_bytes(posix_port *pp, int dir, const nl_u8 *b, long n)
{
    long i;

    if (!pp->trace || n <= 0) return;
    pp->trace_col = 16;     /* each write or read starts a line, with its own time */
    for (i = 0; i < n; i++) {
        if (pp->trace_dir != dir || pp->trace_col >= 16) {
            if (pp->trace_dir) fputc('\n', pp->trace);
            fprintf(pp->trace, "%c %8lu", dir, (unsigned long)pp_now(pp));
            pp->trace_dir = dir;
            pp->trace_col = 0;
        }
        fprintf(pp->trace, " %02X", b[i]);
        pp->trace_col++;
    }
    fflush(pp->trace);
}

static void trace_note(posix_port *pp, const char *what, long value)
{
    if (!pp->trace) return;
    if (pp->trace_dir) fputc('\n', pp->trace);
    fprintf(pp->trace, "# %8lu %s %ld\n", (unsigned long)pp_now(pp), what, value);
    pp->trace_dir = 0;
    fflush(pp->trace);
}

static long pp_write(void *ctx, const nl_u8 *bytes, long n)
{
    posix_port *pp = (posix_port *)ctx;
    long done = 0;
    ssize_t w;

    while (done < n) {
        w = write(pp->fd, bytes + done, (size_t)(n - done));
        if (w < 0) {
            if (errno == EAGAIN || errno == EINTR) { pp_sleep(ctx, 1); continue; }
            return -1;
        }
        done += (long)w;
    }
    trace_bytes(pp, '>', bytes, n);
    return n;
}

static long pp_read(void *ctx, nl_u8 *bytes, long max)
{
    posix_port *pp = (posix_port *)ctx;
    ssize_t r = read(pp->fd, bytes, (size_t)max);

    if (r < 0) {
        if (errno == EAGAIN || errno == EINTR) return 0;
        return -1;
    }
    trace_bytes(pp, '<', bytes, (long)r);
    return (long)r;
}

static int pp_set_speed(void *ctx, long baud)
{
    posix_port *pp = (posix_port *)ctx;
    struct termios t;

    /* Let what has been written leave at the old speed first. */
    tcdrain(pp->fd);
    if (tcgetattr(pp->fd, &t) != 0) return -1;
    if (cfsetspeed(&t, (speed_t)baud) != 0) return -1;
    if (tcsetattr(pp->fd, TCSANOW, &t) != 0) return -1;
    trace_note(pp, "speed", baud);
    return 0;
}

static int pp_set_power(void *ctx, int on)
{
    posix_port *pp = (posix_port *)ctx;
    int lines = TIOCM_DTR | TIOCM_RTS;

    if (ioctl(pp->fd, on ? TIOCMBIS : TIOCMBIC, &lines) != 0) {
        /* A device with no control lines (a pseudo-terminal in a test):
         * carry on without them. A real adapter failing is an error. */
        if (errno != ENOTTY && errno != EINVAL) return -1;
        trace_note(pp, "power (no control lines on this device)", on);
        return 0;
    }
    trace_note(pp, "power", on);
    return 0;
}

int posix_port_open(posix_port *pp, const char *device)
{
    struct termios t;

    memset(pp, 0, sizeof *pp);
    pp->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (pp->fd < 0) return -1;
    if (tcgetattr(pp->fd, &t) != 0) { close(pp->fd); return -1; }
    cfmakeraw(&t);
    t.c_cflag |= (tcflag_t)(CLOCAL | CREAD | CS8);
    t.c_cflag &= (tcflag_t)~(PARENB | CSTOPB | CRTSCTS);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    cfsetspeed(&t, B19200);
    if (tcsetattr(pp->fd, TCSANOW, &t) != 0) { close(pp->fd); return -1; }

    pp->port.ctx = pp;
    pp->port.write = pp_write;
    pp->port.read = pp_read;
    pp->port.set_speed = pp_set_speed;
    pp->port.set_power = pp_set_power;
    pp->port.now_ms = pp_now;
    pp->port.sleep_ms = pp_sleep;
    return 0;
}

void posix_port_close(posix_port *pp)
{
    if (pp->trace && pp->trace_dir) fputc('\n', pp->trace);
    if (pp->fd >= 0) close(pp->fd);
    pp->fd = -1;
}

/*
 *
 * BottleRocket command handling functions.  Only x10_br_out is available to 
 * other programs.  Rest are for it's use.
 *
 * (c) 1999 Tymm Twillman (tymm@acm.org).  Free Software.  LGPL applies.
 *  No warranties expressed or implied.
 *
 * This is for interfacing with the X10 Dynamite wireless transmitter for X10
 *  home automation hardware.
*/
#ifdef __cplusplus
extern C {
#endif


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <limits.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno;
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#include "br_cmd.h"
#include "br_translate.h"

#ifndef TIOCM_FOR_0
#define TIOCM_FOR_0 TIOCM_DTR
#endif

#ifndef TIOCM_FOR_1
#define TIOCM_FOR_1 TIOCM_RTS
#endif

/*
 * These values should be good for pretty much everyone, but you can
 *   try increasing them if you have problems.  Values are in uSec;
 *   PreCmdDelay is the amount of time to hold output lines in
 *   "clock" position before a command, PostCmdDelay is how long
 *   to stay in "clock" position after a command, and InterBitDelay
 *   is how long each bit/clock wiggled out the serial port should
 *   last.
 */

int PreCmdDelay = 300000;   /* empirically found... */
int PostCmdDelay = 300000;
int InterBitDelay = 1400;

static int usec_sleep(long usecs)
{
    /*
     * Sleep for a little while.  Using select() so we don't busy-
     *  wait for this long.
     */

    struct timeval sleeptime;
    int tmperrno;


    sleeptime.tv_sec = usecs / 1000000;
    sleeptime.tv_usec = usecs % 1000000;

    if (select(0, NULL, NULL, NULL, &sleeptime) < 0) {
	tmperrno = errno;
	perror("select");
	errno = tmperrno;
        return -1;
    }

    return 0;
}

static int usec_delay(long usecs)
{
    struct timeval endtime;
    struct timeval currtime;
    int tmperrno;

    /*
     * This way of doing things stolen from Firecracker, by Chris Yokum.
     *   Much better.  What was I thinking before?
     */

    if (gettimeofday(&endtime, NULL) < 0) {
        tmperrno = errno;
        perror("gettimeofday");
        errno = tmperrno;
        return -1;
    }

    endtime.tv_usec += usecs;

    if (endtime.tv_usec > 1000000) {
        endtime.tv_sec++;
        endtime.tv_usec -= 1000000;
    }

    do {
        if (gettimeofday(&currtime, NULL) < 0) {
            tmperrno = errno;
            perror("gettimeofday");
            errno = tmperrno;
            return -1;
        }
    } while (timercmp(&endtime, &currtime, >));

    return 0;
}


static int bits_out(const int fd, const int bits)
{
    /*
     * Send out one command bit; set RTS or DTR (but only one) depending on
     *  value.
     *
     * This now assumes that both RTS and DTR are high; it just clears the one
     *  that we don't want set for this bit.
     */

    int out;
    int tmperrno;


    out = (bits) ? TIOCM_FOR_0:TIOCM_FOR_1;

#ifdef DEBUG
    /*
     * I print these things out funny because that's how I started doing
     *  it and so too bad if it looks weird.
     */

    printf("%d", out >> 1);
#endif

    /* Set RTS, DTR to desired settings */

    if (ioctl(fd, TIOCMBIC, &out) < 0) {
        tmperrno = errno;
        perror("ioctl");
        errno = tmperrno;
        return -1;
    }

    if (usec_delay(InterBitDelay) < 0)
        return -1;
    
    return 0;
}

static int clock_out(const int fd)
{
    /*
     * Send out a "clock pulse" -- both RTS and DTR set; used before/after
     *  command (long pulse) and between command bits (short)
     */

    int out = TIOCM_FOR_0 | TIOCM_FOR_1;
    int tmperrno;

    
#ifdef DEBUG
    printf("%d", out >> 1);
#endif

    if (ioctl(fd, TIOCMBIS, &out) < 0) {
        tmperrno = errno;
        perror("ioctl");
        errno = tmperrno;
        return -1;
    }

    if (usec_delay(InterBitDelay) < 0)
        return -1;
    
    return 0;
}


int x10_br_out(int fd, unsigned char unit, int cmd)
{

    /*
     * Put together the commands to send out.  The basic start and end of
     *  each command is the same; just fill in the little bits in the middle
     *
     * Yeah, it's pretty nasty; I'll probably get around to cleaning this
     *  up soon, but until then you just have to suffer.
     */

    unsigned char cmd_seq[5] = { 0xd5, 0xaa, 0x00, 0x00, 0xad };

    register int i;
    register int j;
    unsigned char byte;
    int out;
    int housecode;
    int device;
    int serial_state;
    int tmperrno;
#ifdef USE_CLOCAL
    struct termios termios;
    struct termios tmp_termios;
#endif

    /*
     * Make sure to set the numeric part of the device address to 0
     *  for dim/bright (they only work per housecode)
     */
    
    if ((cmd == DIM) || (cmd == BRIGHT))
        unit &= 0xf0;

#ifdef USE_CLOCAL

    if (tcgetattr(fd, &termios) < 0) {
        tmperrno = errno;
        perror("ioctl");
        errno = tmperrno;
        return -1;
    }

    tmp_termios = termios;

    tmp_termios.c_cflag |= CLOCAL;

    if (tcsetattr(fd, TCSANOW, &tmp_termios) < 0) {
        tmperrno = errno;
        perror("ioctl");
        errno = tmperrno;
        return -1;
    }

#endif

    /*
     * Save current state of bits we don't want to touch in serial
     *  register
     */
    
    if (ioctl(fd, TIOCMGET, &serial_state) < 0) {
        tmperrno = errno;
        perror("ioctl");
        errno = tmperrno;
        return -1;
    }

    /* Save state of lines to be mucked with
     */

    serial_state &= (TIOCM_FOR_0 | TIOCM_FOR_1);

    /* Figure out which ones we're going to want to clear
     *  when finished (they'll both be high after the last
     *  clock_out)
     */

    serial_state ^= (TIOCM_FOR_0 | TIOCM_FOR_1);

    /*
     * Open with a clock pulse to let the receiver get its wits about 
     */


    housecode = unit >> 4;
    device = unit & 0x0f;

    if ((cmd > MAX_CMD) || (cmd < 0))
        return -1;

    /*
     * Slap together the variable part of a command
     */

    cmd_seq[2] |= housecode_table[housecode] << 4 | device_table[device][0];
    cmd_seq[3] |= device_table[device][1] | cmd_table[cmd];

    /*
     * Set lines to clock and wait, to make sure receiver is ready
     */

    if (clock_out(fd) < 0)
        return -1;

    if (usec_sleep(PreCmdDelay) < 0)
        return -1;

    for (j = 0; j < 5; j++) {
        byte = cmd_seq[j];

#ifdef UGLY_DEBUG
        printf("sending byte: %02x\n", (unsigned int)byte);
#endif

        /*
         * Roll out the bits, following each one by a "clock".
         */

        for (i = 0; i < 8; i++) {
            out = (byte & 0x80) ? 1:0;
            byte <<= 1;
            if ((bits_out(fd, out) < 0) || (clock_out(fd) < 0))
                return -1;
        }
    }

    /*
     * Close with a clock pulse and wait a bit to allow command to complete
     */

    if (clock_out(fd) < 0)
        return -1;
    
    if (usec_sleep(PostCmdDelay) < 0)
        return -1;

   if (ioctl(fd, TIOCMBIC, &serial_state) < 0) {
        tmperrno = errno;
        perror("ioctl");
        errno = tmperrno;
        return -1;
    }

#ifdef USE_CLOCAL
    if (tcsetattr(fd, TCSANOW, &termios) < 0) {
        tmperrno = errno;
        perror("tcsetattr");
        errno = tmperrno;
        return -1;
    }
#endif

    return 0;
}

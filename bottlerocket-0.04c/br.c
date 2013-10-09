
/*
 *
 * br (BottleRocket)
 *
 * Control software for the X10(R) FireCracker wireless computer
 * interface kit.
 *
 * (c) 1999 Ashley Clark (aclark@ghoti.org) and Tymm Twillman (tymm@acm.org)
 *  Free Software.  LGPL applies.
 *  No warranties expressed or implied.
 *
 * Have fun with it and if you do anything really cool, send an email and let us
 * know.
 *
 */

#define VERSION "0.04c"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "br_cmd.h"

#ifdef HAVE_ISSETUGID

/*
 * Thanks to Warner Losh for info on how to do this better
 */

#define ISSETID() (issetugid())
#else
#define ISSETID() (getuid() != geteuid() || getgid() != getegid())
#endif

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#define DIMRANGE 12
#define MAX_COMMANDS 512

#define HOUSENAME(house) (((house < 0) || (house > 15)) ? \
                          '?':"ABCDEFGHIJKLMNOP"[house])
#define DEVNAME(dev) (((dev < 0) || (dev > 16)) ? 0 : dev + 1)
#define CINFO_CLR(cinfo) memset(cinfo, 0, sizeof(br_control_info))
#define SAFE_FILENO(fd) ((fd != STDIN_FILENO) && (fd != STDOUT_FILENO) \
                        && (fd != STDERR_FILENO))

/*
 * Could have device info/commands dynamically allocated, but that's too much
 * trouble, and even this allows for really really obnoxious command lines.
 */

typedef struct {
    int inverse;
    int repeat;
    char *port;
    int fd;
    int numcmds;
    int devs[MAX_COMMANDS];
    char houses[MAX_COMMANDS];
    int dimlevels[MAX_COMMANDS];
    int cmds[MAX_COMMANDS];
} br_control_info;

int Verbose = 0;
char *MyName = "br";

void usage()
{
    fprintf(stderr, "BottleRocket version %s\n", VERSION);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [<options>][<housecode>(<list>) "
      "<native command> ...]\n\n", MyName);
    fprintf(stderr, "  Options:\n");
#ifdef HAVE_GETOPT_LONG
    fprintf(stderr, "  -v, --verbose\t\t\tadd v's to increase verbosity\n");
    fprintf(stderr, "  -x, --port=PORT\t\tset port to use\n");
    fprintf(stderr, "  -c, --house=[A-P]\t\tuse alternate house code "
      "(default \"A\")\n");
    fprintf(stderr, "  -n, --on=LIST\t\t\tturn on devices in LIST\n");
    fprintf(stderr, "  -f, --off=LIST\t\tturn off devices in LIST\n");
    fprintf(stderr, "  -N, --ON\t\t\tturn on all devices in housecode\n");
    fprintf(stderr, "  -F, --OFF\t\t\tturn off all devices in housecode\n");
    fprintf(stderr, "  -d, --dim=LEVEL[,LIST]\tdim devices in housecode to "
      " relative LEVEL\n");
    fprintf(stderr, "  -B, --lamps_on\t\tturn all lamps in housecode on\n");
    fprintf(stderr, "  -D, --lamps_off\t\tturn all lamps in housecode off\n");
    fprintf(stderr, "  -r, --repeat=NUM\t\trepeat commands NUM times "
      "(0 = ~ forever)\n");
    fprintf(stderr, "  -h, --help\t\t\tthis help\n\n");
#else
    fprintf(stderr, "  -v\tverbose (add v's to increase verbosity)\n");
    fprintf(stderr, "  -x\tset port to use\n");
    fprintf(stderr, "  -c\tuse alternate house code (default \"A\")\n");
    fprintf(stderr, "  -n\tturn on devices\n");
    fprintf(stderr, "  -f\tturn off devices\n");
    fprintf(stderr, "  -N\tturn all devices in housecode on\n");
    fprintf(stderr, "  -F\tturn all devices in housecode off\n");
    fprintf(stderr, "  -d\tdim devices in housecode to relative dimlevel\n");
    fprintf(stderr, "  -B\tturn all lamps in housecode on\n");
    fprintf(stderr, "  -D\tturn all lamps in housecode off\n");
    fprintf(stderr, "  -r\trepeat commands <repeats> times (0 basically "
      "means don't stop)\n");
    fprintf(stderr, "  -h\tthis help\n\n");
#endif
    fprintf(stderr, "<list>\t\tis a comma separated list of devices "
      "(no spaces),\n");
    fprintf(stderr, "\t\teach ranging from 1 to 16\n");     
    fprintf(stderr, "<dimlevel>\tis an integer from %d to %d "
      "(0 means no change)\n", -DIMRANGE, DIMRANGE);
    fprintf(stderr, "<housecode>\tis a letter between A and P\n");
    fprintf(stderr, "<native cmd>\tis one of ON, OFF, DIM, BRIGHT, "
      "ALL_ON, ALL_OFF,\n");
    fprintf(stderr, "\t\tLAMPS_ON or LAMPS_OFF\n\n");
    fprintf(stderr, "For native commands, <list> should only be specified "
      "for ON or OFF.\n\n");

}


int checkimmutableport(char *port_source)
{
/*
 * Check to see if the user is allowed to specify an alternate serial port
 */

    if (!ISSETID())
        return 0;

    fprintf(stderr, "%s:  You are not authorized to change the X10 port!\n",
      MyName);
    fprintf(stderr, "%s:  Invalid port assignment %s.\n", MyName, port_source);

    errno = EINVAL;

    return -1;
}


int gethouse(char *house)
{
/*
 * Grab a house code from the command line
 */

    int c;


    c = toupper(house[0]) - 'A';
    if ((strlen(house) > 1) || (c < 0) || (c > 15)) {
        fprintf(stderr, "%s:  House code must be in range [A-P]\n", MyName);
        errno = EINVAL;
        return -1;
    }

    return c;
}


int getdim(char *list, int *dim)
{
/*
 * Get devices that should be dimmed from the command line, and how
 *  much to dim them
 */    
    
    char *end;
    int dev;
    int devs = 0;
    
    *dim = strtol(list, &end, 0);
    
    /*
     * May have more dimlevels when I get a chance to play with variable
     *  dimming
     */

    if (((*end != '\0') && (*end != ',')) 
        || (*dim < -DIMRANGE) 
        || (*dim > DIMRANGE)) 
    {
        fprintf(stderr, "%s:  For dimming either specify just a dim level or "
          "a comma\n",MyName);
        fprintf(stderr, "separated list containing the dim level and the "
          "devices to dim.\n");
        fprintf(stderr, "%s:  Valid dimlevels are numbers between %d and %d.\n",
          MyName, -DIMRANGE, DIMRANGE);
        errno = EINVAL;
        return -1;
    }

    list = end;

    while (*list++) {
        dev = strtol(list, &end, 0);

        if ((dev > 16) 
            || (dev < 1) 
            || ((*end != '\0') && (*end != ','))) 
        {
            fprintf(stderr, "%s:  Devices must be in the range of 1-16.\n", 
              MyName);
            errno = EINVAL;
            return -1;
        }

        devs |= 1 << (dev - 1);

        list = end;
    }

    return devs;
}


int getdevs(char *list)
{
/*
 * Get a list of devices for an operation to be performed on from
 *  the command line
 */

    int devs = 0;
    int dev;
    char *end;


    do {
        dev = strtol(list, &end, 0);

        if ((dev > 16) 
            || (dev < 1) 
            || ((*end != '\0') && (*end != ','))) 
        {
            fprintf(stderr, "%s:  Devices must be in the range of 1-16\n", 
              MyName);
            errno = EINVAL;
            return -1;
        }

        /*
         * Set the bit corresponding to the given device
         */

        devs |= 1 << (dev - 1);

        list = end;
    } while (*list++); /* Skip the , */

    return devs;
}


int br_getunit(char *arg, int *house, int *devs)
{
/*
 * Get units to be accessed from the command line in native BottleRocket style
 */

    if (strlen(arg) < 2) {
        errno = EINVAL;
        return -1;
    }

    if ((*devs = getdevs(arg + 1)) < 0)
        return -1;

    *(arg + 1) = '\0';

    if ((*house = gethouse(arg)) < 0)
        return -1;

    return 0;
}


int br_native_getcmd(char *arg)
{
/*
 * Convert a native BottleRocket command to the appropriate token
 */

    if (!strcasecmp(arg, "ON"))
        return ON;

    if (!strcasecmp(arg, "OFF"))
        return OFF;

    if (!strcasecmp(arg, "DIM"))
        return DIM;

    if (!strcasecmp(arg, "BRIGHT"))
        return BRIGHT;

    if (!strcasecmp(arg, "ALL_ON"))
        return ALL_ON;

    if (!strcasecmp(arg, "ALL_OFF"))
        return ALL_OFF;

    if (!strcasecmp(arg, "LAMPS_ON"))
        return ALL_LAMPS_ON;

    if (!strcasecmp(arg, "LAMPS_OFF"))
        return ALL_LAMPS_OFF;

    fprintf(stderr, "%s:  Command must be one of ON, OFF, DIM, BRIGHT, "
	    "ALL_ON, ALL_OFF, LAMPS_ON or LAMPS_OFF.\n", MyName);
    errno = EINVAL;

    return -1;
}


int process_list(int fd, int house, int devs, int cmd)
{
/*
 * Process and execute on/off commands on a cluster of devices
 */

    unsigned short unit;
    int i;

    /* apply cmd to devices in list */
    
    for (i = 0; i < 16; i++) {
        if (devs & (1 << i)) {
            unit = (unsigned char)((house << 4) | i);
            if (Verbose)
                printf("%s:  Turning %s appliance %c%d\n", MyName,
                  (cmd == ON) ? "on":"off", HOUSENAME(house), DEVNAME(i));
         
             if (x10_br_out(fd, unit, (unsigned char)cmd) < 0)
                 return -1;
        }
    }
    
    return 0;
}


int process_dim(int fd, int house, int devs, int dim)
{
/*
 * Process and execute dimming commands on a housecode or a cluster of
 *  devices
 */

    register int i;
    int unit = (unsigned char)(house << 4);
    int cmd = (dim < 0) ? DIM:BRIGHT;
    int tmpdim;

    
    dim = (dim < 0) ? -dim:dim;
    tmpdim = dim;
    
    if (!devs) {
        if (Verbose)
            printf("%s:  %s lamps in house %c by %d.\n", MyName,
              (cmd == BRIGHT) ? "Brightening":"Dimming", HOUSENAME(house), dim);
        for (; tmpdim; tmpdim--) {
            if (x10_br_out(fd, unit, (unsigned char)cmd) < 0)
                return -1;
        }
    } else {
        for (i = 0; i < 16; i++) {
            if (devs & (1 << i)) {
                if (Verbose)
                    printf("%s:  %s lamp %c%d by %d.\n", MyName,
                      (cmd == BRIGHT) ? "Brightening":"Dimming", 
                      HOUSENAME(house), DEVNAME(i), dim);
                /* Send an ON cmd to select the device this may change later */
                if (x10_br_out(fd, unit | i, ON) < 0)
                    return -1;
                
                for (; tmpdim > 0; tmpdim--)
                    if (x10_br_out(fd, unit, (unsigned char)cmd) < 0)
                        return -1;
                tmpdim = dim;
            }
        }
    }
    
    return 0;
}

int open_port(br_control_info *cinfo)
{
/*
 * Open the serial port that a FireCracker device is (we expect) on
 */

    int tmperrno;

    if (Verbose >= 2)
        printf("%s:  Opening serial port %s.\n", MyName, cinfo->port);

    /*
     * Oh, yeah.  Don't need O_RDWR for ioctls.  This is safer.
     */
        
    if ((cinfo->fd = open(cinfo->port, O_RDONLY | O_NONBLOCK)) < 0) {
	tmperrno = errno;
        fprintf(stderr, "%s: Error (%s) opening %s.\n", MyName, 
          strerror(errno), cinfo->port);
	errno = tmperrno;
        return -1;
    }
    
    /*
     * If we end up with a reserved fd, don't mess with it.  Just to make sure.
     */
    
    if (!SAFE_FILENO(cinfo->fd)) {
        close(cinfo->fd);
        errno = EBADF;
        return -1;
    }

    return 0;
}

int close_port(br_control_info *cinfo)
{
/*
 * Close the serial port when we're done using it
 */

    if (Verbose >= 2)
        printf("%s:  Closing serial port.\n", MyName);

    close(cinfo->fd);

    return 0;
}


int addcmd(br_control_info *cinfo, int cmd, int house, int devs, int dimlevel)
{
    /*
     * Add a command, plus devices for it to act on and other info, to the
     *  list of commands to be executed
     */

    if (cinfo->numcmds >= MAX_COMMANDS) {
        fprintf(stderr, "%s:  Too many commands specified.\n", MyName);
        errno = EINVAL;
        return -1;
    }

    cinfo->cmds[cinfo->numcmds] = cmd;
    cinfo->devs[cinfo->numcmds] = devs;
    cinfo->dimlevels[cinfo->numcmds] = dimlevel;
    cinfo->houses[cinfo->numcmds] = house;

    cinfo->numcmds++;

    return 0;
}

int br_execute(br_control_info *cinfo)
{
/*
 * Run through a list of commands and execute them
 */

    register int i;
    register int repeat = cinfo->repeat;
    int inverse = cinfo->inverse;
    int cmd;

    if (Verbose >= 2)
        printf("%s:  Executing %d commands; repeat = %d, inverse = %d\n", 
          MyName, cinfo->numcmds, repeat, inverse);

    /*
     * Pete and Repeat go into a store, Pete comes out...
     */

    for (; repeat > 0; repeat--) {
        for (i = 0; i < cinfo->numcmds; i++)
        {
            cmd = cinfo->cmds[i];
            if ((cmd == ON) || (cmd == OFF)) {
                cmd = (inverse >= 0) ? cmd : (cmd == OFF) ? ON:OFF;
    
                if (process_list(cinfo->fd, cinfo->houses[i],
                  cinfo->devs[i], cmd) < 0)
                {
                    return -1;
                }
            } else if ((cmd == ALL_ON) || (cmd == ALL_OFF)) {
                cmd = (inverse >= 0) ? cmd : (cmd == ALL_OFF) ? ALL_ON:ALL_OFF;
    
                if (x10_br_out(cinfo->fd, cinfo->houses[i] << 4, cmd) < 0)
                    return -1;
    
            } else if ((cmd == ALL_LAMPS_ON) || (cmd == ALL_LAMPS_OFF)) {
                cmd = (inverse >= 0) ? cmd : (cmd == ALL_LAMPS_OFF) ?
                  ALL_LAMPS_ON:ALL_LAMPS_OFF;
    
                if (x10_br_out(cinfo->fd, cinfo->houses[i] << 4, cmd) < 0)
                    return -1;
    
            } else if (cmd == DIM) {
                if (process_dim(cinfo->fd, cinfo->houses[i], cinfo->devs[i],
                  (inverse >= 0) ? 
                  cinfo->dimlevels[i]:-cinfo->dimlevels[i]) < 0)
                {
                    return -1;
                }
            }
        }
            
        if (inverse) inverse = 0 - inverse;
    }
    
    return 0;
}

int native_br(br_control_info *cinfo, int argc, char *argv[], int optind)
{
/*
 * Get arguments from the command line in native BottleRocket style
 */

    int cmd;
    int house = 0;
    int devs = 0;
    int i;
    
    if (argc - optind < 2) {
        usage();
        errno = EINVAL;
        return -1;
    }

    /*
     * Two arguments at a time; device and command
     */
    
    for (i = optind; i < argc - 1; i += 2) {
        cmd = br_native_getcmd(argv[i + 1]);
        
        switch(cmd) {
            case ON:
                if (br_getunit(argv[i], &house, &devs) < 0)
                    return -1;
                if (addcmd(cinfo, ON, house, devs, 0) < 0)
                    return -1;
                break;
        
            case OFF:
                if (br_getunit(argv[i], &house, &devs) < 0)
                    return -1;
                if (addcmd(cinfo, OFF, house, devs, 0) < 0)
                    return -1;
                break;
        
            case DIM:
                if ((house = gethouse(argv[i])) < 0)
                    return -1;
                if (addcmd(cinfo, DIM, house, 0, -1) < 0)
                    return -1;
                break;
        
            case BRIGHT:
                if ((house = gethouse(argv[i])) < 0)
                    return -1;
                if (addcmd(cinfo, DIM, house, 0, 1) < 0)
                    return -1;
                break;
    
            case ALL_ON:
                if ((house = gethouse(argv[i])) < 0)
                    return -1;
                if (addcmd(cinfo, ALL_ON, house, 0, 1) < 0)
                    return -1;
                break;
        
            case ALL_OFF:
                if ((house = gethouse(argv[i])) < 0)
                    return -1;
                if (addcmd(cinfo, ALL_OFF, house, 0, 1) < 0)
                    return -1;
                break;
        
            case ALL_LAMPS_ON:
                if ((house = gethouse(argv[i])) < 0)
                    return -1;
                if (addcmd(cinfo, ALL_LAMPS_ON, house, 0, 1) < 0)
                    return -1;
                break;
        
            case ALL_LAMPS_OFF:
                if ((house = gethouse(argv[i])) < 0)
                    return -1;
                if (addcmd(cinfo, ALL_LAMPS_OFF, house, 0, 1) < 0)
                    return -1;
                break;
        
            default:        /* How the hell did we end up here? */
                errno = EINVAL;
                return -1;
        }
    }

    if (i != argc) {
        usage();
        errno = EINVAL;
        return -1;
    }

    return 0;
}        

int main(int argc, char **argv)
{
    char *port_source = "at compile time";
    char *tmp_port;
    int opt;
    int house = 0;
    int repeat;
    int devs;
    int dimlevel = 0;
    br_control_info *cinfo = NULL;
    int tmperrno;
    
#ifdef HAVE_GETOPT_LONG    
    int opt_index;
    static struct option long_options[] = {
        {"help",  	no_argument, 	    	0, 'h'},
        {"port",  	required_argument, 	0, 'x'},
        {"repeat",	required_argument, 	0, 'r'},
        {"on", 		required_argument, 	0, 'n'},
        {"off", 	required_argument, 	0, 'f'},
        {"ON", 		no_argument, 		0, 'N'},
        {"OFF", 	no_argument, 		0, 'F'},
        {"dim", 	required_argument, 	0, 'd'},
        {"lamps_on", 	no_argument, 		0, 'B'},
        {"lamps_off", 	no_argument, 		0, 'D'},
        {"inverse", 	no_argument, 		0, 'i'},
	{"house",	required_argument,	0, 'c'},
	{"verbose",	no_argument,		0, 'v'},
	{0, 0, 0, 0}
    };
#endif

#define OPT_STRING	"x:hvr:ic:n:Nf:Fd:BD"
    
    /*
     * It's possible to do an exec without even passing argv[0];
     *  this is just to make sure we don't make any bad
     *  output decisions without accounting for that
     */     
            
    if (argc)
        MyName = argv[0];

    if (argc < 2) {
        usage();
        exit(EINVAL);
    }
    
    if ((cinfo = malloc(sizeof(br_control_info))) == NULL) {
	tmperrno = errno;
        fprintf(stderr, "%s: Error (%s) allocating memory.\n", MyName,
          strerror(errno));
        exit(tmperrno);
    }
    
    CINFO_CLR(cinfo);
    cinfo->port = X10_PORTNAME;
    cinfo->repeat = 1;
    
    if ((tmp_port = getenv("X10_PORTNAME"))) {
        port_source = "in the environment variable X10_PORTNAME";
        if (!checkimmutableport(port_source))
            cinfo->port = tmp_port;
    }
    
#ifdef HAVE_GETOPT_LONG
    while ((opt = getopt_long(argc, argv, OPT_STRING, long_options, &opt_index)) != -1) 
    {
#else
    while ((opt = getopt(argc, argv, OPT_STRING)) != -1) {
#endif        
        switch (opt) {
        case 'x':
            port_source = "on the command line";
            if (checkimmutableport(port_source) < 0)
                exit(errno);
            cinfo->port = optarg;
            break;
        case 'r':
            repeat = atoi(optarg);
            if (!repeat && !isdigit(*optarg)) {
                fprintf(stderr, "%s:  Invalid repeat value specified.\n", 
                  MyName);
                exit(EINVAL);
            }
            cinfo->repeat = (repeat ? repeat:INT_MAX);
            break;
        case 'v':
            if (Verbose++ == 10)
                fprintf(stderr, "\nGet a LIFE already.  "
                  "I've got enough v's, thanks.\n\n");
            break;
        case 'i':
            cinfo->inverse++;    /* no this isn't documented. 
                                  *   your free gift for reading the source.
                                  */
            break;
        case 'c':
            if ((house = gethouse(optarg)) < 0)
                exit(errno);
            break;
        case 'n':
            if ((devs = getdevs(optarg)) < 0)
                exit(errno);
            if (addcmd(cinfo, ON, house, devs, 0) < 0)
                exit(errno);
            break;
        case 'N':
            if (addcmd(cinfo, ALL_ON, house, 0, 0) < 0)
                exit(errno);
            break;
        case 'f':
            if ((devs = getdevs(optarg)) < 0)
                exit(errno);
            if (addcmd(cinfo, OFF, house, devs, 0) < 0)
                exit(errno);
            break;
        case 'F':
            if (addcmd(cinfo, ALL_OFF, house, 0, 0) < 0)
                exit(errno);
            break;
        case 'd':
            if ((devs = getdim(optarg, &dimlevel)) < 0)
                exit(errno);
            if (addcmd(cinfo, DIM, house, devs, dimlevel) < 0)
                exit(errno);
            break;
        case 'B':
            if (addcmd(cinfo, ALL_LAMPS_ON, house, 0, 0) < 0)
                exit(errno);
            break;
        case 'D':
            if (addcmd(cinfo, ALL_LAMPS_OFF, house, 0, 0) < 0)
                exit(errno);
            break;
        case 'h':
            usage();
            exit(0);
        default:
            usage();
            exit(EINVAL);
        }
    }

    if (argc > optind) {
        /*
         * Must be using the native BottleRocket command line...
         */
    
        if (native_br(cinfo, argc, argv, optind) < 0)
            exit(errno);
    }

    if (open_port(cinfo) < 0)
        exit(errno);

    if (br_execute(cinfo) < 0)
        exit(errno);
            
    if (close_port(cinfo) < 0)
        exit(errno);
    
    free(cinfo);

    return 0;
}

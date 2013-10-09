#ifndef X10_BR_CMD_H
#define X10_BR_CMD_H

/*
 * Commands allowed for x10_br_out
 */

#define ON 0
#define OFF 1
#define DIM 2
#define BRIGHT 3 /* upper bit gets masked off; just to tell dim/bright apart */
#define ALL_OFF 4
#define ALL_ON 5
#define ALL_LAMPS_OFF 6
#define ALL_LAMPS_ON 7

int x10_br_out(int /* file desc */, unsigned char /* address */, int /* cmd */);

/*
 * Now these can be set externally; good for when we get config files done
 */

extern int PreCmdDelay;
extern int PostCmdDelay;
extern int InterBitDelay;

#endif

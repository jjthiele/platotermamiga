/**
 * PLATOTermAmiga - A PLATO Terminal for the Commodore Amiga
 * Based on Steve Peltz's PAD
 * 
 * Author: Thomas Cherryhomes <thom.cherryhomes at gmail dot com>
 *
 * io.h - Input/output functions (serial/ethernet)
 */

#ifndef IO_H
#define IO_H

#define XON  0x11
#define XOFF 0x13

typedef struct _MySer
{
  struct MsgPort *writeport;  /** when the writeio is done, reply goes here **/
  struct MsgPort *readport;   /** ibid for the readio **/
  struct IOExtSer *readio, *writeio;
  UBYTE writedata[2048], readdata[2048];  /** data space **/
} MySer;

/**
 * io_init() - Set-up the I/O
 */
void io_init(void);

/**
 * io_send_byte(b) - Send specified byte out
 */
void io_send_byte(unsigned char b);

/**
 * io_main() - The IO main loop
 */
void io_main(void);

/**
 * io_done() - Called to close I/O
 */
void io_done(void);

#endif /* IO_H */

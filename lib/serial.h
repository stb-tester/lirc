/****************************************************************************
** serial.c ****************************************************************
****************************************************************************
*
* common routines for hardware that uses the standard serial port driver
* @ingroup  private_api
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

/**
 * @file serial.h
 * @brief Common routines for hw that uses the standard serial port driver.
 * @author Christoph Bartelmus
 * @ingroup driver_api
 *
 * Here is create_lock and delete_lock which manages the legacy, serial lock files.
 * The other functions are wrappers for the termio(7) IOCTL commands.
 */

/** @addtogroup driver_api
 *  @{
 */


#ifndef _SERIAL_H
#define _SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set the cfmakeraw termio options.
 *
 * @param fd File opened on a serial device.
 * @return 0 on errors, else 1.
 */
int tty_reset(int fd);

/**
 * Set/clear CTS control line.
 *
 * @param enable If true sets CTS, else clears it.
 * @return 0 on errors, else 1.
 */
int tty_setrtscts(int fd, int enable);

/**
 * Set/clear DTR control line.
 *
 * @param fd File opened on a serial device.
 * @param enable If true sets DTR, else clears it.
 * @return 0 on errors, else 1.
 */
int tty_setdtr(int fd, int enable);

/**
 * Set the speed a. k. a. baudrate.
 *
 * @param fd File opened on a serial device.
 * @param baud Speed constant as defined for termios cfsetospeed e.g.,
 *     B19200
 * @return 0 on errors, else 1.
 */
int tty_setbaud(int fd, int baud);

/**
 * Set the character size
 *
 * @param fd File opened on a serial device.
 * @param  Number of data bits:  CS5, CS6, CS7, or CS8.
 * @return 0 on errors, else 1.
 */
int tty_setcsize(int fd, int csize);

/**
 * Creates a lock file of the type /var/local/LCK.. + name
 * @param name Name of the device
 * @return non-zero if successful
 * @see  http://www.pathname.com/fhs/2.2/fhs-5.9.html
 */
int tty_create_lock(const char* name);

/**
 * Delete any legacy lock(s) owned by this process.
 * @return 0 on errors, else 1.
 * @see  http://www.pathname.com/fhs/2.2/fhs-5.9.html
 */
int tty_delete_lock(void);

/**
 * Set RTS and DTR control lines.
 *
 * @param fd File opened on a serial device.
 * @param rts If 0 ignored,  else sets RTS.
 * @param cts If 0 ignored,  else sets CTS.
 * @return 0 on errors, else 1.
 */
int tty_set(int fd, int rts, int dtr);

/**
 * Clear RTS and DTR control lines.
 *
 * @param fd File opened on a serial device.
 * @param rts If 0 ignored,  else clears RTS.
 * @param cts If 0 ignored,  else clears CTS.
 * @return 0 on errors, else 1.
 */
int tty_clear(int fd, int rts, int dtr);

/**
 * Write a single byte to serial device.
 *
 * @param fd File opened on a serial device.
 * @param byte Item to write.
 * @return -1 on errors, else 1.
 */
int tty_write(int fd, char byte);

/**
 * Read a single byte from serial device.
 *
 * @param fd File opened on a serial device.
 * @param byte Pointer to byte to be read.
 * @return -1 on errors, else 1 and data stored in *byte.
 */
int tty_read(int fd, char* byte);


/**
 * Write a single byte and check the echo from remote party. Makes a
 * log printout if these don't match.
 *
 * @param fd File opened on a serial device.
 * @param byte Byte to be written.
 * @return 1 if a byte is successfully written and read, else 0. It's thus
 *     1 even if the echo doesn't match.
 */
int tty_write_echo(int fd, char byte);

/** @} */

#ifdef __cplusplus
}
#endif

#endif

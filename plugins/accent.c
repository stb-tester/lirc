/****************************************************************************
** hw_accent.c *************************************************************
****************************************************************************
*
* LIRC driver for Kanam/Accent serial port remote control.
*
* The Kanam/Accent is a remote control with an IR receiver
* connecting to the serial port. The receiver communicate with the
* host system at 1200 8N1, so the standard serial driver provided
* by the Linux kernel is used.
*
* For each keypress on the remote control, a sequence of 13 or 14
* bytes is transmitted. We can consider just the first 8 bytes as
* significative. Each sequence begins with the three bytes: 0x90
* 0x46 0x42. If a key is held-down, a sequence of zeroes is
* transmitted. The gap between two different full codes is about
* 188500 microseconds. The gap between each zero on a key-hold is
* about 56000 microseconds.
*
* Sometimes the receiver jams, especially on very short key press.
* In this case a uninterrupted stream of zeroes is transmitted,
* without the gap of 56000 us. The stream is interrupted if
* another key is pressed on the remote or if the driver closes and
* reopen the serial port.
*
* Unfortunately the LIRC source code is not well documented, so I
* hope to have guessed well the workflow of lircd. Please, contact
* me if the comments in this source code are not accurate or
* clear.
*
* Author:	Niccolo Rigacci <niccolo@rigacci.org>
*
* Version:	1.1	2007-02-12
*
* Original routines from hw_pixelview.c and hw_pinsys.c.
* First working code for this remote from Leandro Dardini.
*
* Christoph Bartelmus <lirc@bartelmus.de>
* Bart Alewijnse <scarfboy@yahoo.com>
* Leandro Dardini <ldardini@tiscali.it>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyS0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

// Max number of bytes received in a sequence.
#define ACCENT_MAX_READ_BYTES 16
// Only the first bytes of the sequence are meaning.
#define ACCENT_MEANING_BYTES 8
// The meaning bytes are packed into an integer of this length.
#define ACCENT_CODE_LENGTH 64
// Baud rate for the serial port.
#define ACCENT_BAUD_RATE 1200
#define ACCENT_BAUD_RATE_CONST B1200

static const logchannel_t logchannel = LOG_DRIVER;

static unsigned char b[ACCENT_MAX_READ_BYTES];

// Timestamps of keypress start, keypress end and last pressed key.
static struct timeval start, end, last;

// Time (us) of a signal received from the remote control.
static lirc_t signal_length;

// The code of the pressed key and the previous one.
// Type ir_code is __u64.
static ir_code code, last_code = 0;

// Forwards
static int accent_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int accent_open_serial_port(const char* device);
static int accent_init(void);
static int accent_deinit(void);
static char* accent_rec(struct ir_remote* remotes);


const struct driver hw_accent = {
	.name		= "accent",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= ACCENT_CODE_LENGTH,
	.init_func	= accent_init,
	.deinit_func	= accent_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= accent_rec,
	.decode_func	= accent_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.resolution	= 300,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_accent, (const struct driver*)NULL };


//-------------------------------------------------------------------------
// This function is called by the LIRC daemon during the transform of a
// received code into an lirc event.
//
// It gets the global variable code (remote keypress code).
//
// It returns:
//      prep            Code prefix (zero for this LIRC driver)
//      codep           Code of keypress
//      postp           Trailing code (zero for this LIRC dirver)
//      repeat_flagp    True if the keypress is a repeated keypress
//      remaining_gapp  Extimated time gap remaining before next code?
//-------------------------------------------------------------------------
int accent_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	log_trace("Entering accent_decode(), code = %016llx\n", code);

	log_trace("accent_decode() is calling map_code()");
	if (!map_code(remote, ctx, 0, 0, ACCENT_CODE_LENGTH, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	log_trace("Exiting accent_decode()");

	return 1;
}

//-------------------------------------------------------------------------
// Lock and initialize the serial port.
// This function is called by the LIRC daemon when the first client
// registers itself.
// Return 1 on success, 0 on error.
//-------------------------------------------------------------------------
int accent_init(void)
{
	log_trace("Entering accent_init()");

	// Calculate the time length of a remote signal (in microseconds):
	// (bits + total_stop_bits) * 1000000 / bitrate
	signal_length = (drv.code_length + (drv.code_length / 8)) * 1000000 / ACCENT_BAUD_RATE;

	if (!tty_create_lock(drv.device)) {
		log_error("Could not create the lock file");
		return 0;
	}
	drv.fd = accent_open_serial_port(drv.device);
	if (drv.fd  < 0) {
		log_error("Could not open the serial port");
		accent_deinit();
		return 0;
	}
	return 1;
}

//-------------------------------------------------------------------------
// Close and release the serial line.
//-------------------------------------------------------------------------
int accent_deinit(void)
{
	log_trace("Entering accent_deinit()");
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

//-------------------------------------------------------------------------
// Receive a code (bytes sequence) from the remote.
// This function is called by the LIRC daemon when I/O is pending
// from a registered client, e.g. irw.
//-------------------------------------------------------------------------
char* accent_rec(struct ir_remote* remotes)
{
	char* m;
	int i, j;

	log_trace("Entering accent_rec()");

	// Timestamp of the last pressed key.
	last = end;
	// Timestamp of key press start.
	gettimeofday(&start, NULL);

	// Loop untill read ACCENT_MAX_READ_BYTES or sequence timeout.
	for (i = 0; i < ACCENT_MAX_READ_BYTES; i++) {
		// The function accent_rec() is called when some data
		// is already available to read, so we don't wait on
		// the first byte.
		if (i > 0) {
			// Each of the following bytes must be
			// received within some timeout.  7500 us is
			// the standard time for receiving a byte
			// (wait at least this time) 56000 us is the
			// min time gap between two code sequences
			// (don't wait so much)
			if (waitfordata(45000) == 0) {
				// waitfordata() timed out: the
				// sequence is complete.
				log_trace("waitfordata() timeout waiting for byte %d", i);
				break;
			}
		}
		// Some data available to read.
		if (read(drv.fd, &b[i], 1) == -1) {
			log_perror_err("read() failed at byte %d", i);
			return NULL;
		}
		log_trace("read() byte %d: %02x", i, b[i]);
	}                       // End for

	// Timestamp of key press end.
	gettimeofday(&end, NULL);

	// The bytes sequence is complete, check its validity.
	log_trace("Received a sequence of %d bytes", i);

	// Just one byte with zero value: repeated keypress?
	if (i == 1 && b[0] == 0) {
		if (last_code && (start.tv_sec - last.tv_sec < 2)) {
			// A previous code exists and the time gap is
			// lower than 2 seconds.
			log_info("Received repeated key");
			code = last_code;
			tcflush(drv.fd, TCIFLUSH);
			m = decode_all(remotes);
			return m;
		}
		log_trace("Previos code not set, invalid repeat key");
		last_code = 0;
		return NULL;
	}
	// Sequence too short?
	if (i < ACCENT_MEANING_BYTES) {
		log_notice("Invalid sequence: too short");
		last_code = 0;
		return NULL;
	}
	// A valid code begins with bytes 0x90 0x46 0x42
	// and it is long not more than ACCENT_MEANING_BYTES.
	if (b[0] == 0x90 && b[1] == 0x46 && b[2] == 0x42) {
		code = 0;
		if (sizeof(code) >= ACCENT_MEANING_BYTES) {
			// We have plenty of space to store the full sequence.
			code |= b[0];
			code <<= 8;
			code |= b[1];
			code <<= 8;
			code |= b[2];
			code <<= 8;
			code |= b[3];
			code <<= 8;
			code |= b[4];
			code <<= 8;
			code |= b[5];
			code <<= 8;
			code |= b[6];
			code <<= 8;
			code |= b[7];
		} else {
			// No much space, keep only the differentiating part.
			code |= b[3];
			code <<= 8;
			code |= b[4];
			code <<= 8;
			code |= b[5];
			code <<= 8;
			code |= b[6];
		}
		log_trace("sizeof(code) = %d", sizeof(code));
		log_trace("Received code -> 0x%016llx", code);
		last_code = code;
		tcflush(drv.fd, TCIFLUSH);
		m = decode_all(remotes);
		return m;
	}
	// Sometimes the receiver goes crazy, it starts to send to the
	// serial line a sequence of zeroes with no pauses at all.
	// This jam terminates only if the user press a new button on
	// the remote or if we close and re-open the serial port.
	if (i == ACCENT_MAX_READ_BYTES) {
		for (j = 0; j < ACCENT_MAX_READ_BYTES; j++)
			if (b[j] != 0)
				break;
		if (j == ACCENT_MAX_READ_BYTES) {
			// All the received bytes are zeroes, without gaps.
			log_warn("Receiver jam! Reopening the serial port");
			close(drv.fd);
			drv.fd = accent_open_serial_port(drv.device);
			if (drv.fd < 0) {
				log_error("Could not reopen the serial port");
				raise(SIGTERM);
			}
			last_code = 0;
			return NULL;
		}
	}
	// Should never reach this point.
	log_notice("Received an invalid sequence");
	for (j = 0; j < i; j++)
		log_trace(" b[%d] = %02x", j, b[j]);
	last_code = 0;
	return NULL;
}

//-------------------------------------------------------------------------
// Open the serial line and set the discipline (do the low level work).
// Return the file descriptor or -1 on error.
//-------------------------------------------------------------------------
int accent_open_serial_port(const char* device)
{
	int fd;
	struct termios options;

	log_debug("Entering accent_open_serial_port(), device = %s", device);

	// Open the serial device.
	fd = open(device, O_RDWR | O_NONBLOCK | O_NOCTTY | O_SYNC);
	if (fd  < 0) {
		log_perror_err("Could not open the serial port");
		return -1;
	}
	// Get the parameters associated with the serial line.
	if (tcgetattr(fd, &options) < 0) {
		log_error("Could not get serial port attributes");
		log_perror_err("tcgetattr() failed");
		return -1;
	}
	// Set the line in raw mode (no control chars, etc.)
	cfmakeraw(&options);
	// Apply the changes after all the output has been transmitted.
	// Discard input before the change is made.
	if (tcsetattr(fd, TCSAFLUSH, &options) < 0) {
		log_error("Could not set serial port with cfmakeraw()");
		log_perror_err("tcsetattr() failed");
		return -1;
	}
	// Gets the parameters associated with the serial line.
	if (tcgetattr(fd, &options) < 0) {
		log_error("Could not get serial port attributes");
		log_perror_err("tcgetattr() failed");
		return -1;
	}
	// Set input and output baud rate to 1200.
	cfsetispeed(&options, ACCENT_BAUD_RATE_CONST);
	cfsetospeed(&options, ACCENT_BAUD_RATE_CONST);
	// Disable RTS/CTS (hardware) flow control.
	options.c_cflag &= ~CRTSCTS;
	// Set one stop bit.
	options.c_cflag &= ~CSTOPB;
	// Ignore modem control lines.
	options.c_cflag |= CLOCAL;
	// Enable receiver.
	options.c_cflag |= CREAD;
	// Disable parity checking for input.
	options.c_cflag &= ~PARENB;
	if (tcsetattr(fd, TCSAFLUSH, &options) < 0) {
		log_error("Could not set serial port line discipline");
		log_perror_err("tcsetattr() failed");
		return -1;
	}
	// Discards data received but not read.
	if (tcflush(fd, TCIFLUSH) < 0) {
		log_error("Could not flush input buffer");
		log_perror_err("tcflush() failed");
		return -1;
	}

	return fd;
}

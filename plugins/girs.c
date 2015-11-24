/* Copyright (C) 2015 Bengt Martensson
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Library General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
*/

// Coding style:
// Declaring variables "far away" from their usage is IMHO not good programming.
// Instead, they are declared immediately before usage, violating old C,
// but compliant with C99 and later. GCC with default options has no warnings.
// In this context, the convention of leaving an empty line between
// "the declarations" and "the statements", found in some coding guidelines,
// makes no sense, and is thus not observerved.
// Also not compliant with Brian's and Dennis' C is the usage of the C++ comment
// character.

// In this file, readability has been deliberately sacrificed in order to
// keep the lines punch card compatible, leading to some pretty awful lines.

// Driver parameters:
/*
* substitute_0_frequency
	Define if the problem of
	<a href="https://sourceforge.net/p/lirc/tickets/132/">this ticket</a>
	is present in the used lircd, and the old behavior is wanted.
	If the frequency is 0, and this parameters is non-zero,
	the frequency will be substituted by this value, typically 38000.
	Not needed if all your remotes have explict, non-zero frequencies.
	Defining this makes it impossible to send non-modulated signals.
 * connectled
	If non-zero, the LED with this number will be turned
	on the first time the driver is accessed and stays
	on until the device is closed,
	normally at the end of the Lircd run.
* initedled
	If non-zero, the LED with this number will be turned
	on by the init function ("has been inited", motivating the "funny" name),
	and turned on by the deinit function.</dd>
 * transmitled
	f non-zero, the LED with this number will be turned
	on by the send function and turned on by the deinit function.
 * drop_dtr_when_initing
	If non-zero, the "DTR line" will be lowered for 100 ms when
	making the first connect, causing most Arduinos to reset.
 */

#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <lirc/lirc_config.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

/** Devices enumerated by device_hint. */
#define DEVICE_HINT \
	"/bin/sh ls /dev/ttyACM* /dev/ttyUSB* /dev/arduino* 2>/dev/null"

/* Timeouts, all in microseconds */
#define TIMEOUT_FLUSH	100

/** Timeout for first answer from initial connect */
#define TIMEOUT_INITIAL 10000

/** Timeout from command to answer */
#define TIMEOUT_COMMAND 250
#define TIMEOUT_SEND	2000

#define DTR_WAIT	100

#define BAUDRATE	115200
#define EOL		"\r"
#define SUCCESS_RESPONSE "OK"
#define TIMEOUT_RESPONSE "."
#define RECEIVE_COMMAND "receive"
#define TRANSMIT_COMMAND "transmit"
#define LED_COMMAND	"led"
#define LCD_COMMAND	"lcd"

/* Longest expected line */
#define LONG_LINE_SIZE 1000

#define NO_SYNCRONIZE_ATTEMPTS 10

/** Max durations expected on read */
#define MAXDATA 500

// Define if the device sends \r\n as line ending.
#define CRLF_FROM_DEVICE

static const logchannel_t logchannel = LOG_DRIVER;

typedef struct {
	int fd;
	int read_pending;
	int send_pending;
	int command_names_on_lcd;
	int lcd;
	int led;
	int connectled;
	int initedled;
	int transmitled;
	int substitute_0_frequency;
	int drop_dtr_when_initing;
	int receive;
	int transmit;
	int transmitters;
	unsigned int transmitter_mask;
	char version[LONG_LINE_SIZE]; // Use to indicate valid device
	char driver_version[LONG_LINE_SIZE];
} girs_t;

static girs_t dev = {
	.fd = -1,
	.read_pending = 0,
	.send_pending = 0,
	.command_names_on_lcd = 0,
	.lcd = 0,
	.led = 0,
	.connectled = 0,
	.initedled = 0,
	.transmitled = 0,
	.substitute_0_frequency = 0,
	.drop_dtr_when_initing = 0,
	.receive = 0,
	.transmit = 0,
	.transmitters = 0,
	.transmitter_mask = 0,
	.version = "",
	.driver_version = ""
};

static int init(void);
static int deinit(void);
static int send(struct ir_remote* remote, struct ir_ncode* code);
static char* receive(struct ir_remote* remotes);
static lirc_t readdata(lirc_t timeout);
static int setLed(int ledNo, int status);
static int girs_close(void);
static int girs_open(const char* path);
static int setLcd(const char* message);
static int drvctl(unsigned int cmd, void* arg);

// This driver determines features, send_mode, and rec_mode dynamically during
// runtime, using the Girs modules command.
// Since lirc-lsplugins is not smart enough to
// understand this, just give some realistic default values.

const struct driver hw_girs = {
	.device		= "/dev/ttyACM0",
	.fd		= -1,
	.features	= LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= init,
	.deinit_func	= deinit,
	.send_func	= send,
	.rec_func	= receive,
	.decode_func	= receive_decode, // defined in receive.c
	.drvctl_func	= drvctl,
	.readdata	= readdata,
	.name		= "girs",
	.resolution	= 50,
	.api_version	= 3,
	.driver_version = "2015-10-09",
	.info		= "See file://" PLUGINDOCS "/girs.html",
	.open_func	= girs_open,  // does not open, just string copying
	.close_func	= girs_close, // when really terminating the program
	.device_hint    = DEVICE_HINT,
};

const struct driver* hardwares[] = {
	&hw_girs,
	(const struct driver*) NULL
};

static int is_valid(void)
{
	return dev.fd >= 0 && dev.version[0] != '\0';
}

static int min(int x, int y)
{
	return x < y ? x : y;
}

static int girs_close(void)
{
	if (dev.fd >= 0) {
		if (is_valid()) {
			setLed(dev.connectled, 0);
			setLed(dev.initedled, 0);
			setLed(dev.transmitled, 0);
			setLcd("Closing!");
		}
		close(dev.fd);
	}
	dev.fd = -1;
	dev.version[0] = '\0';
	tty_delete_lock();
	return 0;
}

/**
 * Driver control.
 *
 * Process the --driver-option key:value pairs.
 *
 *  clocktick:value
 *	Set the timing resolution to specified value.
 *
 * \retval 0	Success.
 * \retval !=0	drvctl error.
 */
static int drvctl(unsigned int cmd, void* arg)
{
	if (cmd == LIRC_SET_TRANSMITTER_MASK) {
		if (!dev.transmitters) {
			log_error("girs: Current firmware does not support "
				  "setting transmitter mask.");
			return DRV_ERR_NOT_IMPLEMENTED;
		}
		log_warn("setting of transmitter mask accepted, "
			 "but not yet implemented: 0x%x, ignored.",
			 *(unsigned int*) arg);
		dev.transmitter_mask = *(unsigned int*) arg;
		return 0;
	} else if (cmd == DRVCTL_SET_OPTION) {
		struct option_t* opt = (struct option_t*) arg;
		long value = strtol(opt->value, NULL, 10);

		if (strcmp(opt->key, "command_names_on_lcd") == 0) {
			if (value < 0 || value > 1) {
				log_error(
					"invalid command_names_on_lcd: %d, ignored.",
					value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.command_names_on_lcd = value;
		} else if (strcmp(opt->key, "substitute_0_frequency") == 0) {
			if (value < 0) {
				log_error(
					"invalid substitute_0_frequency: %d, ignored.",
					value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.substitute_0_frequency = value;
		} else if (strcmp(opt->key, "connectled") == 0) {
			if (value < 0 || value > 8) {
				log_error("invalid connectled: %d, ignored.",
					  value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.connectled = value;
		} else if (strcmp(opt->key, "initedled") == 0) {
			if (value < 0 || value > 8) {
				log_error("invalid initedled: %d, ignored.",
					  value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.initedled = value;

		} else if (strcmp(opt->key, "transmitled") == 0) {
			if (value < 0 || value > 8) {
				log_error("invalid transmitled: %d, ignored.",
					  value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.transmitled = value;
		} else if (strcmp(opt->key, "drop_dtr_when_initing") == 0) {
			if (value < 0 || value > 1) {
				log_error("invalid drop_dtr_when_initing: %d, ignored.",
					  value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.drop_dtr_when_initing = value;
		} else {
			log_error("unknown key \"%s\", ignored.",
				  opt->key);
			return DRV_ERR_BAD_OPTION;
		}
		return 0;
	} else
		return DRV_ERR_NOT_IMPLEMENTED;
}

/**
 *
 * @param buf
 * @param count
 * @param timeout in milliseconds; 0 means infinite timeout.
 * @return number or characters read, or -1 if error
 */
static ssize_t read_with_timeout(char* buf, size_t count, long timeout)
{
	ssize_t rc;
	size_t numread = 0;
	struct pollfd pfd = {.fd = dev.fd, .events = POLLIN, .revents = 0};

	rc = read(dev.fd, buf, count);

	if (rc > 0)
		numread += rc;

	while ((rc == -1 && errno == EAGAIN) || (rc >= 0 && numread < count)) {

		rc = poll(&pfd, 1, timeout ? (int) timeout : -1);

		if (rc == 0)
			/* timeout */
			break;
		else if (rc == -1)
			/* continue for EAGAIN case */
			continue;

		rc = read(dev.fd, ((char*)buf) + numread, count - numread);

		if (rc > 0)
			numread += rc;
	}
	return (numread == 0) ? -1 : numread;
}

static int readline(char* buf, size_t size, long timeout)
{
	ssize_t rc = 0;

	buf[0] = '\0';
	int noread = 0;

	while (1) {
		char c;

		rc = read_with_timeout(&c, 1, timeout);
		if (rc == -1) {
			// timeout
			if (noread) {
				if (timeout == 0)
					continue;
				else {
					log_warn("girs: timeout with partially "
						 "read string \"%s\", discarded",
						 buf);
					buf[0] = '\0';
					break;
				}
			} else {
				log_debug("girs: timeout in readline");
				break;
			}
		}


		if (rc == 1 && ((c == '\n')
#ifndef CRLF_FROM_DEVICE
			|| (c == '\r')
#endif
			)) {
			if (noread == 0)
				continue;
			else {
				buf[min(noread, size - 1)] = '\0';
				log_trace("girs: readline returned \"%s\"",
					  buf);
				break;
			}
		}
#ifdef CRLF_FROM_DEVICE
		if (c == '\r') {
			continue;
		}
#endif
		if (rc == 1) {
			if (noread < size - 1) {
				buf[noread] = c;
			} else if (noread == size - 1) {
				buf[noread] = '\0';
				log_error("girs: realine buffer full: \"%s\"",
					  buf);
				// but we keep on looking for an end-of-line
			} else
				;

			noread++;
		}
	}
	return rc > 0;
}

static void readflush(void)
{
	char c;

	log_trace("girs: flushing the input");
	while (read_with_timeout(&c, 1, TIMEOUT_FLUSH) == 1)
		log_trace1("girs: flushing \"%c\"", c);
}

static int sendcommand(const char *command)
{
	if (command[0] != '\0') {
		int nbytes = write(dev.fd, command, strlen(command)); // FIXME

		if (nbytes != strlen(command)) {
			log_error("girs: could not write command \"%s\"",
				  command);
			return 0;
		}
		logprintf(nbytes > 1 ? LIRC_TRACE : LIRC_TRACE1,
			  "girs: written command \"%s\"", command);
	}
	return 1;
}

static int sendcommandln(const char *command)
{
	int success = sendcommand(command) && sendcommand(EOL);

	if (!success)
		return 0;
	tcdrain(dev.fd);
	return 1;
}

static int sendcommand_answer(const char *command, char *buf, int len)
{
	int status = sendcommandln(command);

	if (!status) {
		buf[0] = '\0';
		return 0;
	}
	return readline(buf, len, TIMEOUT_COMMAND);
}

/**
 *
 * @param command
 * @return 1 on success, 0 on wrong answer, -1 on timeout.
 */
static int sendcommand_ok(const char *command)
{
	log_trace1("girs: sendcommand_ok \"%s\"", command);
	char answer[LONG_LINE_SIZE];
	int success = sendcommand_answer(command, answer, LONG_LINE_SIZE);

	if (success) {
		log_debug("girs: command \"%s\" returned \"%s\"",
			  command, answer);
		return strncmp(answer, SUCCESS_RESPONSE,
			strlen(SUCCESS_RESPONSE)) == 0;
	} else {
		log_debug("girs: command \"%s\" returned error",
			  command);
		return -1;
	}
}

/**
 * "press return" until we get "OK".
 * @return 1 if success
 */
static int syncronize(void)
{
	log_debug("girs: syncronizing");
	dev.read_pending = 0;
	dev.send_pending = 0;
	int i;

	for (i = 0; i < NO_SYNCRONIZE_ATTEMPTS; i++) {
		int res = sendcommand_ok(" ");
		//if (res == -1)
			//return 0;
		if (res == 1) {
			log_debug("girs: syncronized!");
			return 1;
		}
	}
	log_trace("girs: failed syncronizing after "
		  STR(NO_SYNCRONIZE_ATTEMPTS) " attempts");
	return 0;
}

static int setLed(int ledNo, int status)
{
	if (dev.led && ledNo > 0) {
		char cmd[20];

		sprintf(cmd, "%s %d %d", LED_COMMAND, ledNo, status);
		return sendcommand_ok(cmd);
	} else
		return 1;
}

static int setLcd(const char* message)
{
	if (dev.lcd) {
		char cmd[strlen(LCD_COMMAND) + strlen(message) + 2];

		sprintf(cmd, "%s %s", LCD_COMMAND, message);
		return sendcommand_ok(cmd);
	} else
		return 1;
}

static int enable_receive(void)
{
	//syncronize();
	int success = sendcommandln(RECEIVE_COMMAND);

	if (success) {
		readflush();
		dev.read_pending = 1;
	} else {
		log_error("girs: sending " RECEIVE_COMMAND " failed");
	}
	return success;
}

static void drop_dtr(void)
{
	log_debug("girs: dropping DTR to reset the device");
	tty_setdtr(drv.fd, 0);
	usleep(DTR_WAIT * 1000);
	// turn on DTR
	tty_setdtr(drv.fd, 1);
}

static lirc_t readdata(lirc_t timeout)
{
	static unsigned int data[MAXDATA];
	static unsigned int data_ptr = 0;
	static unsigned int data_length = 0;

	if (!dev.receive)
		// this should not happen
		return 0;

	log_trace("girs readdata, timeout = %d", timeout);
	if (data_length == data_ptr/* && timeout > 0*/) {
		// Nothing to deliver, try to read some new data
		if (!dev.read_pending) {
			int success = enable_receive();

			if (!success) {
				log_debug("readdata FAILED");
				return 0;
			}

		}
		char buf[5 * MAXDATA];

		while (1) {
			int success = readline(buf, 5 * MAXDATA, timeout);

			if (!success) {
				log_debug("readdata 0 (timeout)");
				// no need to restart receive
				return 0;
			}
			dev.read_pending = 0;
			if (strncmp(buf, TIMEOUT_RESPONSE,
				strlen(TIMEOUT_RESPONSE)) != 0)
				// Got something that is not timeout; go on
				break;

			log_debug("readdata timeout from hardware, continuing");
			enable_receive();
		}
		int i = 0;
		const char* token;

		for (token = strtok(buf, " +-");
			token != NULL;
			token = strtok(NULL, " +-")) {
			if (i < MAXDATA) {
				errno = 0;
				unsigned int x;
				int status = sscanf(token, "%u", &x);

				if (status != 1 || errno) {
					log_error("Could not parse %s as unsigned",
						  token);
					enable_receive();
					return 0;
				}
				data[i] = x;
				i++;
			} else {
				// TODO
			}
		}
		data_ptr = 0;
		data_length = i;

		enable_receive();
	}

	if (data_length == data_ptr)
		return 0;
	unsigned int x = data[data_ptr++];

	if (x >= PULSE_BIT) {
		// TODO
	}
	if (data_ptr & 1)
		x |= PULSE_BIT;

	log_debug("readdata %d %d",
		  !!(x & PULSE_BIT), x & PULSE_MASK);
	return (lirc_t) x;
}

static void decode_modules(char* buf)
{
	char *token;

	dev.receive = 0;
	drv.rec_mode = 0;
	drv.features = 0;
	for (token = strtok(buf, " "); token; token = strtok(NULL, " ")) {
		if (strcasecmp(token, "receive") == 0) {
			log_info("girs: receive module found");
			dev.receive = 1;
			drv.rec_mode = LIRC_MODE_MODE2;
			drv.features |= LIRC_CAN_REC_MODE2;
		} else if (strcasecmp(token, "transmit") == 0) {
			log_info("girs: transmit module found");
			dev.transmit = 1;
			drv.send_mode = LIRC_MODE_PULSE;
			drv.features |= LIRC_CAN_SEND_PULSE
				| LIRC_CAN_SET_SEND_CARRIER;
		} else if (strcasecmp(token, "led") == 0) {
			log_info("girs: LED module found");
			dev.led = 1;
		} else if (strcasecmp(token,
			"lcd") == 0) {
			log_info("girs: LCD module found");
			dev.lcd = 1;
		} else if (strcasecmp(token, "transmitters") == 0) {
			log_info("girs: transmitters module found");
			dev.transmitters = 1;
			drv.features |= LIRC_CAN_SET_TRANSMITTER_MASK;
		} else {
			log_debug("girs: unknown module \"%s", token);
		}
	}
}

static int init(void)
{
	char buf[LONG_LINE_SIZE];

	log_trace1("girs: init");
	if (is_valid()) {
		drv.fd = dev.fd;
	} else {
		if (access(drv.device, R_OK) != 0) {
			log_debug("girs: cannot access %s",
				  drv.device);
			return 0;
		}
		if (!tty_create_lock(drv.device)) {
			log_error("girs: could not create lock files");
			return 0;
		}
		drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
		if (drv.fd < 0) {
			log_error("girs: could not open %s", drv.device);
			tty_delete_lock();
			return 0;
		}
		if (!tty_reset(drv.fd)) {
			log_error("girs: could not reset tty");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}
		if (!tty_setbaud(drv.fd, BAUDRATE)) {
			log_error("girs: could not set baud rate");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}
		if (!tty_setcsize(drv.fd, 8)) {
			log_error("girs: could not set csize");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}
		if (!tty_setrtscts(drv.fd, 0)) {
			log_error("girs: could not disable hardware flow");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}

		if (dev.drop_dtr_when_initing)
			drop_dtr();
		dev.fd = drv.fd;

		int success = readline(buf, LONG_LINE_SIZE, TIMEOUT_INITIAL);

		if (!success) {
			log_warn("girs: no response from device, "
				 "making another try");
			drop_dtr();
			success = readline(buf, LONG_LINE_SIZE,
				TIMEOUT_INITIAL);
			if (!success)
				log_error("girs: no response from device, "
					   "giving up");
		}
		if (success) {
			success = syncronize();
			if (!success) {
				log_error("girs: cannot syncronize");
			}
		}
		if (success) {
			success = sendcommand_answer("version", dev.version,
				LONG_LINE_SIZE);
			if (success) {
				strcpy(dev.driver_version,
					hw_girs.driver_version);
				strcat(dev.driver_version, "/");
				strcat(dev.driver_version, dev.version);
			} else {
				log_error("girs: cannot get version");
			}
		}
		if (success) {
			success = sendcommand_answer("modules", buf,
				LONG_LINE_SIZE);
			if (!success) {
				log_error("girs: cannot get modules");
			} else
				decode_modules(buf);
		}

		if (!success) {
			log_error("girs: Could not open Girs device at %s",
				drv.device);
			girs_close();
			tty_delete_lock();
			return 0;
		}
		log_info("Version \"%s\"", dev.version);
	}
	drv.driver_version = dev.driver_version;
	setLcd("Lirc connected");
	setLed(dev.connectled, 1);
	setLed(dev.initedled, 1);
	rec_buffer_init();
	send_buffer_init();
	readflush();
	return dev.receive ? enable_receive() : 1;
}

// Almost the default open
static int girs_open(const char* path)
{
	static char buff[LONG_LINE_SIZE];

	if (path == NULL) {
		if (drv.device == NULL)
			drv.device = hw_girs.device;
	} else {
		strncpy(buff, path, sizeof(buff) - 1);
		drv.device = buff;
	}
	log_info("girs_open: Initial device: %s", drv.device);
	return 0;
}

static int deinit(void)
{
	log_debug("girs: deinit");
	if (is_valid()) {
		syncronize(); // interrupts reception
		//setLcd("Sleeping...");

		setLed(dev.connectled, 1);
		setLed(dev.initedled, 0);
		setLed(dev.transmitled, 0);
		readflush();
	}
	drv.fd = -1;
	//tty_delete_lock(); // ???
	return 1;
}


static char* receive(struct ir_remote* remotes)
{
	if (!dev.receive)
		// probably should not happen
		return NULL;

	log_debug("girs_receive");
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

// NOTE: In the Lirc model, lircd takes care of the timing between intro and
// repeat etc., NOT the driver. The timing is therefore critical.
static int send(struct ir_remote* remote, struct ir_ncode* code)
{
	if (!dev.transmit)
		// probably should not happen
		return 0;
	if (!send_buffer_put(remote, code))
		return 0;

	int length = send_buffer_length(); // odd!
	const lirc_t* signals = send_buffer_data();

	char buf[LONG_LINE_SIZE];

	if (dev.read_pending)
		syncronize(); // kill possible ongoing receive

	// for timing reasons, do not turn the LED on and off every time
	if (!dev.send_pending)
		setLed(dev.transmitled, 1);

	if (dev.command_names_on_lcd && !dev.send_pending) {
		sprintf(buf, "%s\n%s", remote->name, code->name);
		setLcd(buf);
	}
	dev.send_pending = 1;


	int freq = (remote->freq == 0 && dev.substitute_0_frequency)
	? freq = dev.substitute_0_frequency
	: remote->freq;

	// no_sends, frequency, intro_length, repeat_length, end_length
	sprintf(buf, "send 1 %d %d 0 0", freq, length+1);
	int i;

	for (i = 0; i < length; i++) {
		char b[10];

		sprintf(b, " %d", (unsigned int) signals[i]);
		strcat(buf, b);
	}
	// Girs requires the last duration to be a space, however, Lirc thinks
	// differently. Just add a 1 microsecond space.
	strcat(buf, " 1");

	sendcommandln(buf);
	int success = readline(buf, LONG_LINE_SIZE, TIMEOUT_SEND);

	//setLed(dev.transmitled, 0);
	return success;
}

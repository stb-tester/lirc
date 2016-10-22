/* Copyright (C) 2015, 2016 Bengt Martensson
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
 * drop_dtr_when_initing
 *	If non-zero, if using serial device,
 *      the "DTR line" will be lowered for 100 ms when
 *	making the first connect, causing most Arduinos to reset.
 */

#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/socket.h>
#include <netdb.h>
#include <lirc/lirc_config.h>

#include "lirc_driver.h"
#include "lirc/serial.h"
#include "lirc/curl_poll.h"

/** Devices enumerated by device_hint. */
#define DEVICE_HINT \
	"/bin/sh ls /dev/ttyACM* /dev/ttyUSB* /dev/arduino* 2>/dev/null"

#define DRIVER_NAME "girs"
#define DEFAULT_DEVICE "/dev/ttyACM0"
#define DRIVER_RESOLUTION 50
#define DRIVER_VERSION "2016-08-19"

#define DEFAULT_TCP_PORT "33333"

/* Timeouts, all in microseconds */
#define TIMEOUT_FLUSH	1

/** Timeout for first answer from initial connect */
#define TIMEOUT_INITIAL_SERIAL 5000
#define TIMEOUT_INITIAL_TCP 500

/** Timeout from command to answer */
#define TIMEOUT_COMMAND 250
#define TIMEOUT_SEND	500

#define DTR_WAIT	100

#define BAUDRATE	115200
#define EOL		"\r"
#define SUCCESS_RESPONSE "OK"
#define TIMEOUT_RESPONSE "."
#define RECEIVE_COMMAND "receive"
#define TRANSMIT_COMMAND "transmit"

// Min value of ending timeout in the sense of LIRC_GET_MIN_TIMEOUT, in ms.
#define GIRS_MIN_TIMEOUT 1000 // 1 ms, fairly arbitrary

// Max value of ending timeout in the sense of LIRC_GET_MAX_TIMEOUT, in ms.
#define GIRS_MAX_TIMEOUT 1000000 // 1 s, fairly arbitrary

/* Longest expected line */
#define LONG_LINE_SIZE 1000
#define SMALLSTRINGSIZE 20

#define NO_SYNCRONIZE_ATTEMPTS 10

/** Max number of durations expected on read */
#define MAXDATA 500

// Define if the device sends \r\n as line ending.
#define CRLF_FROM_DEVICE

/**
 * Weird dummy extra gap added in front of the first signal,
 * needed by the Lirc's decode.
 */
#define SILLY_INITIAL_GAP 1000000

static const logchannel_t logchannel = LOG_DRIVER;

/**
 * Type of connection to the Girs server.
 */
typedef enum {
	none,
	serial,
	tcp
} connection_t;

typedef struct {
	int fd;
	int read_pending;
	int send_pending;
	connection_t connection;
	int drop_dtr_when_initing;
	int receive; // Implements the receive modle
	int transmit; // Implements the transmit modle
	int transmitters;// Implements the transmitter modle
	int parameters; // Implements the parameters module
	int last_sent_timeout; // the last timeout value sent to the hardware
	int initialized;
	int report_timeouts; // If true "report timeouts"
	unsigned int transmitter_mask;
	char version[LONG_LINE_SIZE]; // Use to indicate valid device
	char driver_version[LONG_LINE_SIZE];
} girs_t;

static girs_t dev = {
	.fd = -1,
	.read_pending = 0,
	.send_pending = 0,
	.connection = serial,
	.drop_dtr_when_initing = 1,
	.receive = 0,
	.transmit = 0,
	.transmitters = 0,
	.parameters = 0,
	.last_sent_timeout = -1,
	.initialized = 0,
	.report_timeouts = 0,
	.transmitter_mask = 0,
	.version = "",
	.driver_version = ""
};

static int init(void);
static int deinit(void);
static int send_ir(struct ir_remote* remote, struct ir_ncode* code);
static char* receive(struct ir_remote* remotes);
static lirc_t readdata(lirc_t timeout);
static int girs_close(void);
static int girs_open(const char* path);
static int drvctl(unsigned int cmd, void* arg);
static int decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int sendcommand_answer(const char *command, char *buf, int len);
static int syncronize(void);
static int enable_receive(void);

// This driver determines features, send_mode, and rec_mode dynamically during
// runtime, using the Girs modules command.
// Since lirc-lsplugins is not smart enough to
// understand this, just give some realistic default values.

const struct driver hw_girs = {
	.device		= DEFAULT_DEVICE,
	.fd		= -1,
	.features	= LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= init,
	.deinit_func	= deinit,
	.send_func	= send_ir, // previously called send
	.rec_func	= receive,
	.decode_func	= decode,
	.drvctl_func	= drvctl,
	.readdata	= readdata,
	.name		= DRIVER_NAME,
	.resolution	= DRIVER_RESOLUTION,
	.api_version	= 3,
	.driver_version = DRIVER_VERSION,
	.info		= "See file://" PLUGINDOCS "/" DRIVER_NAME ".html",
	.open_func	= girs_open,  // does not open, just string copying
	.close_func	= girs_close, // when really terminating the program
	.device_hint    = DEVICE_HINT,
};

const struct driver* hardwares[] = {
	&hw_girs,
	(const struct driver*) NULL
};

static int decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	log_trace(DRIVER_NAME " decode: enter");
	int res = receive_decode(remote, ctx);

	log_trace(DRIVER_NAME " decode returned: %d", res);
	return res;
}

static int is_valid(void)
{
	return dev.fd >= 0 && dev.version[0] != '\0';
}

static int min(int x, int y)
{
	return x < y ? x : y;
}

// Public function through hw_girs
static int girs_close(void)
{
	log_debug("girs_close called");
	if (dev.fd >= 0)
		close(dev.fd);

	dev.fd = -1;
	dev.version[0] = '\0';
	if (dev.connection == serial)
		tty_delete_lock();
	dev.connection = none;
	return 0;
}

/**
 * Driver control.
 *
 * \retval 0	Success.
 * \retval !=0	drvctl error.
 */
// Public function through hw_girs
static int drvctl(unsigned int cmd, void* arg)
{
	switch (cmd) {
	case LIRC_SET_TRANSMITTER_MASK:
		// May be implemented in the future
		if (!dev.transmitters) {
			log_error(DRIVER_NAME
				": Current firmware does not support setting transmitter mask.");
			return DRV_ERR_NOT_IMPLEMENTED;
		}
		log_warn(DRIVER_NAME
			"setting of transmitter mask accepted, but not yet implemented: 0x%x, ignored.",
			*(unsigned int*) arg);
		dev.transmitter_mask = *(unsigned int*) arg;
		break;
	case LIRC_SET_REC_TIMEOUT:
		if (!dev.parameters)
			return DRV_ERR_NOT_IMPLEMENTED;
		int value = *(int*) arg;
		int millisecs = value / 1000;

		// Do not send the parameter command if it is not necessary,
		// since it interrupts the reception-
		if (millisecs != dev.last_sent_timeout) {
			if (dev.read_pending)
				syncronize();
			char command[LONG_LINE_SIZE];

			snprintf(command, LONG_LINE_SIZE, "parameter receiveending %d", millisecs);
			char answer[LONG_LINE_SIZE];

			snprintf(answer, LONG_LINE_SIZE, "receiveending=%d", millisecs);
			int success = sendcommand_answer(command, answer, LONG_LINE_SIZE);

			if (!success)
				return DRV_ERR_BAD_STATE; // ???
			log_info(DRIVER_NAME ": setting timeout to %d ms", millisecs);
			enable_receive();
			dev.last_sent_timeout = millisecs;
		}
		break;
	case LIRC_GET_MIN_TIMEOUT:
		if (!dev.parameters)
			return DRV_ERR_NOT_IMPLEMENTED;
		*(int*) arg = GIRS_MIN_TIMEOUT;
		break;
	case LIRC_GET_MAX_TIMEOUT:
		if (!dev.parameters)
			return DRV_ERR_NOT_IMPLEMENTED;
		*(int*) arg = GIRS_MAX_TIMEOUT;
		break;
	case LIRC_SET_REC_TIMEOUT_REPORTS:
		dev.report_timeouts = *(int*) arg;
		break;
	case DRVCTL_SET_OPTION:
	{
		struct option_t* opt = (struct option_t*) arg;
		long value = strtol(opt->value, NULL, 10);

		if (strcmp(opt->key, "drop_dtr_when_initing") == 0) {
			if (value < 0 || value > 1) {
				log_error(DRIVER_NAME
					": invalid drop_dtr_when_initing: %d, ignored.",
					value);
				return DRV_ERR_BAD_VALUE;
			}
			dev.drop_dtr_when_initing = (int) value;
		} else {
			log_error("unknown key \"%s\", ignored.", opt->key);
			return DRV_ERR_BAD_OPTION;
		}
	}
		break;
	default:
		return DRV_ERR_NOT_IMPLEMENTED;
	}
	return 0; // success
}

/**
 *
 * @param buf
 * @param count
 * @param timeout in milliseconds; 0 means infinite timeout.
 * @return number or characters read, or -1 if error
 */
static ssize_t read_with_timeout(char* buf, size_t count, int timeout)
{
	ssize_t rc;
	size_t numread = 0;
	struct pollfd pfd = {.fd = dev.fd, .events = POLLIN, .revents = 0};

	rc = curl_poll(&pfd, 1, timeout ? timeout : -1);
	if (rc == 0)
		return -1;
	rc = read(dev.fd, buf, count);

	if (rc > 0)
		numread += rc;

	while ((rc == -1 && errno == EAGAIN) || (rc >= 0 && numread < count)) {

		rc = curl_poll(&pfd, 1, timeout ? timeout : -1);

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
	return (numread == 0) ? -1 : (ssize_t) numread;
}

static int readline(char* buf, size_t size, int timeout)
{
	ssize_t rc = 0;

	buf[0] = '\0';
	unsigned int noread = 0;

	while (1) {
		char c;

		rc = read_with_timeout(&c, 1, timeout);
		if (rc == -1) {
			// timeout
			if (noread) {
				if (timeout == 0)
					continue;
				else {
					log_warn(DRIVER_NAME
						": timeout with partially read string \"%s\", discarded",
						 buf);
					buf[0] = '\0';
					break;
				}
			} else {
				log_debug(DRIVER_NAME ": timeout in readline");
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
				log_trace(DRIVER_NAME
					": readline returned \"%s\"",
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
				log_warn(DRIVER_NAME
					": readline buffer full: \"%s\"",
					buf);
				// but we keep on looking for an end-of-line
			} else {
			}

			noread++;
		}
	}
	return rc > 0;
}

static void readflush(void)
{
	char c;

	log_trace(DRIVER_NAME ": flushing the input");
	while (read_with_timeout(&c, 1, TIMEOUT_FLUSH) == 1)
		log_trace1(DRIVER_NAME ": flushing \"%c\"", c);
}

static int sendcommand(const char *command)
{
	if (command[0] != '\0') {
		ssize_t nbytes = write(dev.fd, command, strlen(command)); // FIXME

		if (nbytes != (ssize_t) strlen(command)) {
			log_error(DRIVER_NAME
				": could not write command \"%s\"",
				command);
			return 0;
		}
		log_trace1(DRIVER_NAME ": written command \"%s\"", command);
	}
	return 1;
}

static int sendcommandln(const char *command)
{
	char buf[strlen(command) + strlen(EOL) + 1];

	strncpy(buf, command, strlen(command)+1);
	strncat(buf, EOL, strlen(EOL));
	int success = sendcommand(buf);

	if (!success)
		return 0;

	if (dev.connection == serial)
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
 * Sends the command given as argument with line ending appended,
 * and waits for answer, assumed to be "OK".
 * @param command
 * @return 1 on success, 0 on wrong answer (not equals to "OK"), -1 on timeout.
 */
static int sendcommand_ok(const char *command)
{
	log_trace1(DRIVER_NAME ": sendcommand_ok \"%s\"", command);
	char answer[LONG_LINE_SIZE];
	int success = sendcommand_answer(command, answer, LONG_LINE_SIZE);

	if (success) {
		log_trace1(DRIVER_NAME ": command \"%s\" returned \"%s\"",
			  command, answer);
		return strncmp(answer, SUCCESS_RESPONSE,
			strlen(SUCCESS_RESPONSE)) == 0;
	} else {
		log_debug(DRIVER_NAME ": command \"%s\" returned error",
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
	log_debug(DRIVER_NAME ": synchronizing");
	dev.read_pending = 0;
	dev.send_pending = 0;
	int i;

	for (i = 0; i < NO_SYNCRONIZE_ATTEMPTS; i++) {
		int res = sendcommand_ok("");

		if (res == 1) {
			log_debug(DRIVER_NAME ": synchronized!");
			return 1;
		}
	}
	log_debug(DRIVER_NAME ": failed synchronizing after "
		  STR(NO_SYNCRONIZE_ATTEMPTS) " attempts");
	return 0;
}

static int enable_receive(void)
{
	//synchronize();
	int success = sendcommandln(RECEIVE_COMMAND);

	if (success) {
		readflush();
		dev.read_pending = 1;
	} else {
		log_error(DRIVER_NAME ": sending " RECEIVE_COMMAND " failed");
	}
	return success;
}

static void drop_dtr(void)
{
	log_debug(DRIVER_NAME ": dropping DTR to reset the device");
	tty_setdtr(drv.fd, 0);
	usleep(DTR_WAIT * 1000);
	// turn on DTR
	tty_setdtr(drv.fd, 1);
}

// Public function through hw_girs
static lirc_t readdata(lirc_t timeout)
{
	static unsigned int data[MAXDATA];
	static unsigned int data_ptr = 0;
	static unsigned int data_length = 0;
	static int initialized = 0;

	if (!dev.receive) {
		log_error(DRIVER_NAME ": internal error");
		return 0;
	}

	log_trace2(DRIVER_NAME ": readdata, timeout = %d", timeout);
	if (data_length == data_ptr/* && timeout > 0*/) {
		// Nothing to deliver, try to read some new data
		if (!dev.read_pending) {
			int success = enable_receive();

			if (!success) {
				log_debug("readdata FAILED");
				return 0;
			}

		}
		char buf[LONG_LINE_SIZE];

		while (1) {
			int success = readline(buf, LONG_LINE_SIZE, timeout);

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
			initialized = 0;
			return 0;
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
					log_error(DRIVER_NAME
						": Could not parse %s as unsigned",
						  token);
					enable_receive();
					return 0;
				}
				data[i] = x;
				i++;
			} else {
				log_warn(DRIVER_NAME
					": Signal had more than %d entries, ignoring the excess",
					MAXDATA);
				break;
			}
		}
		data_ptr = 0;
		data_length = i;

		enable_receive();
	}

	unsigned int x;

	if (!initialized) {
		// The Lirc decoder expects every signal to start with a
		// gap to be thrown away!!!
		log_debug("girs: initial silly gap");
		initialized = 1;
		x = SILLY_INITIAL_GAP;
	} else {
		if (data_ptr >= MAXDATA)
			return 0;

		x = data[data_ptr];

		// Fix if too large (> 16.7 seconds)
		if (x > LIRC_VALUE_MASK)
			x = LIRC_VALUE_MASK;

		// Mark as PULSE if appropriate (otherwise it is SPACE)
		if ((data_ptr & 1) == 0)
			x |= PULSE_BIT;

#ifdef REPORT_TIMEOUT
		if (dev.report_timeouts && (data_ptr == data_length - 1))
			// Last value, mark it as timeout
			x |= LIRC_MODE2_TIMEOUT;
#endif

		data_ptr++;
	}

	log_trace(DRIVER_NAME ": readdata %d %d", (x & LIRC_MODE2_MASK) >> 24,
		x & PULSE_MASK);
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
			log_info(DRIVER_NAME ": receive module found");
			dev.receive = 1;
			drv.rec_mode = LIRC_MODE_MODE2;
			drv.features |= LIRC_CAN_REC_MODE2;
		} else if (strcasecmp(token, "transmit") == 0) {
			log_info(DRIVER_NAME ": transmit module found");
			dev.transmit = 1;
			drv.send_mode = LIRC_MODE_PULSE;
			drv.features |= LIRC_CAN_SEND_PULSE
				| LIRC_CAN_SET_SEND_CARRIER;
		} else if (strcasecmp(token, "transmitters") == 0) {
			log_info(DRIVER_NAME ": transmitters module found");
			dev.transmitters = 1;
			drv.features |= LIRC_CAN_SET_TRANSMITTER_MASK;
		} else if (strcasecmp(token, "parameters") == 0) {
			log_info(DRIVER_NAME ": parameters module found");
			dev.parameters = 1;
			drv.features |= LIRC_CAN_SET_REC_TIMEOUT;
		} else {
			log_debug(DRIVER_NAME ": unknown module \"%s", token);
		}
	}
}

static void kick_device(void)
{
	if (dev.connection == serial && dev.drop_dtr_when_initing)
		drop_dtr();
}

static int initialize_serial(void)
{
		if (access(drv.device, R_OK) != 0) {
			log_debug(DRIVER_NAME ": cannot access %s",
				  drv.device);
			return 0;
		}
		if (!tty_create_lock(drv.device)) {
			log_error(DRIVER_NAME ": could not create lock files");
			return 0;
		}
		drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
		if (drv.fd < 0) {
			log_error(DRIVER_NAME ": could not open %s",
				drv.device);
			tty_delete_lock();
			return 0;
		}
		if (!tty_reset(drv.fd)) {
			log_error(DRIVER_NAME ": could not reset tty");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}
		if (!tty_setbaud(drv.fd, BAUDRATE)) {
			log_error(DRIVER_NAME ": could not set baud rate");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}
		if (!tty_setcsize(drv.fd, 8)) {
			log_error(DRIVER_NAME ": could not set csize");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}
		if (!tty_setrtscts(drv.fd, 0)) {
			log_error(DRIVER_NAME
				": could not disable hardware flow");
			close(drv.fd);
			tty_delete_lock();
			return 0;
		}

	kick_device();

	return 1;
}

static int initialize_tcp(void)
{
	const char* colon = strchr(drv.device, ':');
	char  ipname[strlen(drv.device)+1];
	char  portnumber[SMALLSTRINGSIZE]; // yes, not an int or short!

	if (colon != NULL) {
		strncpy(ipname, drv.device, colon - drv.device);
		strncpy(portnumber, colon + 1, strlen(colon));
	} else {
		strncpy(ipname, drv.device, strlen(drv.device) + 1);
		strncpy(portnumber, DEFAULT_TCP_PORT, SMALLSTRINGSIZE-1);
	}

	/* This is essentially the code from getaddrinfo(3) */
	/* Obtain address(es) matching host/port */
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0; /* Any protocol */

	struct addrinfo *result;
	int status = getaddrinfo(ipname, portnumber, &hints, &result);

	if (status != 0) {
		log_error("getaddrinfo: %s", gai_strerror(status));
		return 0;
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */
	int sock;
	struct addrinfo *rp;

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock == -1)
			continue;

		if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
			log_info("Successful connect to %s:%s",
				ipname, portnumber);
			break; /* Success */
		}
		close(sock);
	}

	if (rp == NULL) {
		/* No address succeeded */
		log_error("Could not connect to %s:%s", ipname, portnumber);
		return 0;
	}
	drv.fd = sock;
	dev.fd = drv.fd;
	freeaddrinfo(result);
	return 1;
}

static int initialize(void)
{
	dev.connection
		= (drv.device[0] == '\0'
		|| drv.device[0] == '/'
		|| strncmp(drv.device, "auto", 4) == 0)
		? serial : tcp;

	int success = dev.connection == serial
		? initialize_serial() : initialize_tcp();

	if (!success)
		return success;

	dev.fd = drv.fd;
	char buf[LONG_LINE_SIZE];

	//if (dev.connection == serial) // quirk in Arduino ethernet library
	success = readline(buf, LONG_LINE_SIZE,
		dev.connection == serial ? TIMEOUT_INITIAL_SERIAL
		: TIMEOUT_INITIAL_TCP);

	if (!success) {
		log_warn(DRIVER_NAME ": no response from device, making another try");
		kick_device();
		success = syncronize();
		if (!success) {
			log_error(DRIVER_NAME ": cannot synchronize");
		}
	}
	if (success) {
		success = sendcommand_answer("version", dev.version,
			LONG_LINE_SIZE);
		if (success) {
			strncpy(dev.driver_version, hw_girs.driver_version, LONG_LINE_SIZE-1);
			strncat(dev.driver_version, "/", 1);
			strncat(dev.driver_version, dev.version, strlen(dev.version));
		} else {
			log_error(DRIVER_NAME ": cannot get version");
		}
	}
	if (success) {
		success = sendcommand_answer("modules", buf,
			LONG_LINE_SIZE);
		if (!success) {
			log_error(DRIVER_NAME ": cannot get modules");
		} else
			decode_modules(buf);
	}

	if (!success) {
		log_error(DRIVER_NAME
			": Could not open Girs device at %s",
			drv.device);
		girs_close();
		tty_delete_lock();
		return 0;
	}
	log_info("girr: Found version \"%s\"", dev.version);
	return 1;
}

// Public function through hw_girs
static int init(void)
{
	log_trace1(DRIVER_NAME ": init");
	if (is_valid())
		drv.fd = dev.fd;
	else {
		int status = initialize();

		if (!status)
			return status;
	}

	drv.driver_version = dev.driver_version;
	rec_buffer_init();
	send_buffer_init();
	readflush();
	dev.initialized = 0;
	return dev.receive ? enable_receive() : 1;
}

// Almost the default open
// Public function through hw_girs
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

// Public function through hw_girs
static int deinit(void)
{
	log_debug(DRIVER_NAME ": deinit");
	if (is_valid()) {
		syncronize(); // interrupts reception
		readflush();
	}
	drv.fd = -1;
	return 1;
}


// Public function through hw_girs
static char* receive(struct ir_remote* remotes)
{
	if (!dev.receive) {
		log_error(DRIVER_NAME ": internal error");
		return NULL;
	}

	log_debug("girs_receive");
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

// NOTE: In the Lirc model, lircd takes care of the timing between intro and
// repeat etc., NOT the driver. The timing is therefore critical.

// Public function through hw_girs
// Previously called send, rename due to name clash with socket.h
static int send_ir(struct ir_remote* remote, struct ir_ncode* code)
{
	if (!dev.transmit) {
		// probably should not happen
		log_error(DRIVER_NAME ": Internal error");
		return 0;
	}
	if (!send_buffer_put(remote, code))
		return 0;

	int length = send_buffer_length(); // odd!
	const lirc_t* signals = send_buffer_data();

	char buf[LONG_LINE_SIZE];

	if (dev.read_pending)
		syncronize(); // kill possible ongoing receive

	dev.send_pending = 1;

	int freq = remote->freq;

	if (freq == 0)
		// Just to be on the safe side...
		log_info(DRIVER_NAME
			": frequency 0 found. If this is not intended, fix corresponding lircd.conf file");

	// no_sends, frequency, intro_length, repeat_length, end_length
	snprintf(buf, LONG_LINE_SIZE, "send 1 %d %d 0 0", freq, length+1);
	int i;

	for (i = 0; i < length; i++) {
		char b[SMALLSTRINGSIZE];

		snprintf(b, SMALLSTRINGSIZE - 1, " %d", (unsigned int) signals[i]);
		strncat(buf, b, SMALLSTRINGSIZE - 1);
	}

	// Girs requires the last duration to be a space, however, Lirc thinks
	// differently. Just add a 1 microsecond space.
	strncat(buf, " 1", 2);

	sendcommandln(buf);
	int success = readline(buf, LONG_LINE_SIZE, TIMEOUT_SEND);
	int enable_receive_success = dev.receive ? enable_receive() : 1;

	return success && enable_receive_success;
}

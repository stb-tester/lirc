/****************************************************************************
** irtoy.c **********************************************************
****************************************************************************
*
* Routines for USB Infrared Toy receiver/transmitter in sampling mode
*
* Copyright (C) 2011 Peter Kooiman <pkooiman@gmail.com>
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

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "lirc_driver.h"
#include "lirc/serial.h"


#define IRTOY_MINFWVERSION 20

#define IRTOY_UNIT 21.3333
#define IRTOY_LONGSPACE 1000000


const unsigned char IRTOY_COMMAND_TXSTART[] = { 0x24, 0x25, 0x26, 0x03 };
static const unsigned char cmdIOwrite = 0x30; // Sets the IO pins to ground (0) or +5volt (1).
static const unsigned char cmdIOdirection = 0x31; // Sets the IO pins to input (1) or output (0).
#define IRTOY_COMMAND_RESET 0
#define IRTOY_COMMAND_SMODE_ENTER 's'
#define IRTOY_COMMAND_VERSION 'v'

#define IRTOY_REPLY_XMITCOUNT 't'
#define IRTOY_REPLY_XMITSUCCESS 'C'
#define IRTOY_REPLY_VERSION 'V'
#define IRTOY_REPLY_SAMPLEMODEPROTO 'S'

#define IRTOY_LEN_XMITRES 4
#define IRTOY_LEN_VERSION 4
#define IRTOY_LEN_SAMPLEMODEPROTO 3

#define IRTOY_TIMEOUT_READYFORDATA 1000000
#define IRTOY_TIMEOUT_FLUSH 20000
#define IRTOY_TIMEOUT_SMODE_ENTER 500000
#define IRTOY_TIMEOUT_VERSION 500000

// To aid debugging, attach LEDs to the pins below. They will light up
// under certain circumstances, helping to find out what the driver does.
// If not needed, the performance penalty is believed to be very small.

/** This LED lights when device is held open. */
static const int openPin = 5; // RA5

/** This LED lights when the device is listening for inputs. */
static const int receivePin = 3; // RA3

/** This LED LED lights during IR signal transmission. */
static const int sendingPin = 4; // RA4

struct tag_irtoy_t {
	int	hwVersion;
	int	swVersion;
	int	protoVersion;
	int	fd;
	int	awaitingNewSig;
	int	pulse;
};

typedef struct tag_irtoy_t irtoy_t;

static const logchannel_t logchannel = LOG_DRIVER;

static irtoy_t* dev = NULL;

static unsigned char rawSB[WBUF_SIZE * 2 + 2];

/* exported functions  */
static int init(void);
static int deinit(void);
static int send(struct ir_remote* remote, struct ir_ncode* code);
static char* receive(struct ir_remote* remotes);
static int decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static lirc_t readdata(lirc_t timeout);


const struct driver hw_usbirtoy = {
	.name		= "irtoy",
	.device		= "/dev/ttyACM0",
	.features	= LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= init,
	.deinit_func	= deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= send,
	.rec_func	= receive,
	.decode_func	= decode,
	.drvctl_func	= NULL,
	.readdata	= readdata,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "See file://" PLUGINDOCS "/irtoy.html",
	.device_hint    = "/dev/ttyACM*",
};


const struct driver* hardwares[] = {
	&hw_usbirtoy,
	(const struct driver*)NULL
};


static int decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	int res;

	log_trace("decode: enter");

	res = receive_decode(remote, ctx);

	log_trace("decode: %d", res);

	return res;
}

static unsigned int IOdirections = (unsigned int) -1;
static unsigned int IOdata = 0U;

int send3(unsigned char cmd, unsigned int data)
{
	unsigned char array[3];
	int res;

	array[0] = cmd;
	array[1] = (unsigned char) ((data >> 8) & 0xff);
	array[2] = (unsigned char) (data & 0xff);
	res = write(dev->fd, array, 3);
	if (res != 3)
		log_error("irtoy_setIOData: couldn't write command");
	return res == 3;
}

static int setIOData(void)
{
	return send3(cmdIOdirection, IOdirections) && send3(cmdIOwrite, IOdata);
}

static int setPin(unsigned int pin, int state)
{
	unsigned int mask = 1 << pin;

	IOdirections &= ~mask;
	if (state)
		IOdata |= mask;
	else
		IOdata &= ~mask;
	return setIOData();
}

static ssize_t
read_with_timeout(int fd, void* buf, size_t count, long to_usec)
{
	ssize_t rc;
	size_t numread = 0;
	struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};

	rc = read(fd, (char*)buf, count);

	if (rc > 0)
		numread += rc;

	while ((rc == -1 && errno == EAGAIN) || (rc >= 0 && numread < count)) {

		rc = curl_poll(&pfd, 1, to_usec / 1000);

		if (rc == 0)
			/* timeout */
			break;
		else if (rc == -1)
			/* continue for EAGAIN case */
			continue;

		rc = read(fd, ((char*)buf) + numread, count - numread);

		if (rc > 0)
			numread += rc;
	}
	return (numread == 0) ? -1 : numread;
}


static int irtoy_readflush(irtoy_t* dev, long timeout)
{
	int res;
	char c;

	while ((res = read_with_timeout(dev->fd, &c, 1, timeout)) == 1)
		;
	if (res != 0)
		return -1;
	else
		return 0;
}


static lirc_t irtoy_read(irtoy_t* dev, lirc_t timeout)
{
	lirc_t data;
	int res;
	unsigned char dur[2];

	if (!waitfordata(timeout))
		return 0;

	// lircd expects a space as start of the next transmission, not
	// just at the end of the last one.
	// irrecord however likes to see a space at the end of the signal
	// We remember if we saw the 0xFFFF timeout from the usbtoy and
	// send a long space both after last signal and at start of next signal
	// From usb irtoy:
	//  <signal...><lastpulse> [usbtoy timeout duration] 0xFFFF
	//  [however long it takes before next signal]
	//  <firstpulse><signal..>
	// We return:
	// <signal><lastpulse> [usbtoy timeout duration] LONGSPACE
	// [however long it takes before next signal]
	// LONGSPACE <firstpulse><signal>AA

	if (dev->awaitingNewSig) {
		log_trace("new signal after large space");
		dev->pulse = 1;
		dev->awaitingNewSig = 0;
		return IRTOY_LONGSPACE;
	}
	res = read_with_timeout(dev->fd, dur, 2, 0);
	if (res != 2) {
		log_error("irtoy_read: could not get 2 bytes");
		return 0;
	}
	log_trace2("read_raw %02x%02x", dur[0], dur[1]);
	if (dur[0] == 0xff && dur[1] == 0xff) {
		dev->awaitingNewSig = 1;
		return IRTOY_LONGSPACE;
	}
	data = (lirc_t)(IRTOY_UNIT * (double)(256 * dur[0] + dur[1]));
	log_trace2("read_raw %d", data);

	if (dev->pulse)
		data = data | PULSE_BIT;
	dev->pulse = !(dev->pulse);

	return data;
}


static lirc_t readdata(lirc_t timeout)
{
	lirc_t data = irtoy_read(dev, timeout);

	if (data)
		log_trace("readdata %d %d",
			  !!(data & PULSE_BIT), data & PULSE_MASK);
	return data;
}


static int irtoy_getversion(irtoy_t* dev)
{
	int res;
	char buf[16];
	int vNum;

	irtoy_readflush(dev, IRTOY_TIMEOUT_FLUSH);


	buf[0] = IRTOY_COMMAND_VERSION;
	res = write(dev->fd, buf, 1);

	if (res != 1) {
		log_error("irtoy_getversion: couldn't write command");
		return 0;
	}

	res = read_with_timeout(dev->fd, buf,
				IRTOY_LEN_VERSION,
				IRTOY_TIMEOUT_VERSION);
	if (res != IRTOY_LEN_VERSION) {
		log_error("irtoy_getversion: couldn't read version");
		log_error("please make sure you are using firmware v20" \
			  " or higher");
		return 0;
	}

	buf[IRTOY_LEN_VERSION] = 0;

	log_trace("irtoy_getversion: Got version %s", buf);

	if (buf[0] != IRTOY_REPLY_VERSION) {
		log_error("irtoy_getversion: invalid response %02X", buf[0]);
		log_error("please make sure you are using firmware v20" \
			  " or higher");
		return 0;
	}

	vNum = atoi(buf + 1);
	dev->hwVersion = vNum / 100;
	dev->swVersion = vNum % 100;
	return 1;
}


static int irtoy_reset(irtoy_t* dev)
{
	int res;
	char buf[16];

	buf[0] = IRTOY_COMMAND_RESET;
	res = write(dev->fd, buf, 1);

	if (res != 1) {
		log_error("irtoy_reset: couldn't write command");
		return 0;
	}

	irtoy_readflush(dev, IRTOY_TIMEOUT_FLUSH);

	return 1;
}


static int irtoy_enter_samplemode(irtoy_t* dev)
{
	int res;
	char buf[16];

	buf[0] = IRTOY_COMMAND_SMODE_ENTER;
	res = write(dev->fd, buf, 1);

	if (res != 1) {
		log_error("irtoy_enter_samplemode: couldn't write command");
		return 0;
	}

	res = read_with_timeout(dev->fd, buf,
				IRTOY_LEN_SAMPLEMODEPROTO,
				IRTOY_TIMEOUT_SMODE_ENTER);
	if (res != IRTOY_LEN_SAMPLEMODEPROTO) {
		log_error("irtoy_enter_samplemode: Can't read command result");
		return 0;
	}

	buf[IRTOY_LEN_SAMPLEMODEPROTO] = 0;
	if (buf[0] != IRTOY_REPLY_SAMPLEMODEPROTO) {
		log_error("irtoy_enter_samplemode: invalid response %02X",
			  buf[0]);
		return 0;
	}

	log_trace("irtoy_reset: Got protocol %s", buf);
	dev->protoVersion = atoi(buf + 1);
	return 1;
}


static irtoy_t* irtoy_hw_init(int fd)
{
	irtoy_t* dev = (irtoy_t*)malloc(sizeof(irtoy_t));

	if (dev == NULL) {
		log_error("init: out of memory");
		return NULL;
	}

	memset(dev, 0, sizeof(irtoy_t));

	dev->awaitingNewSig = 1;
	dev->fd = fd;
	dev->pulse = 1;

	irtoy_readflush(dev, IRTOY_TIMEOUT_FLUSH);

	if (!irtoy_reset(dev) || !irtoy_getversion(dev) ||
	    !irtoy_enter_samplemode(dev)) {
		free(dev);
		dev = NULL;
		return NULL;
	}
	return dev;
}

static int init_device(void)
{
	if (access(drv.device, R_OK) != 0) {
		log_debug("irtoy: cannot access %s", drv.device);
		return 0;
	}
	if (!tty_create_lock(drv.device)) {
		log_error("irtoy: could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("irtoy: could not open %s", drv.device);
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("irtoy: could not reset tty");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 115200)) {
		log_error("irtoy: could not set baud rate");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}
	if (!tty_setcsize(drv.fd, 8)) {
		log_error("irtoy: could not set csize");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}
	if (!tty_setrtscts(drv.fd, 1)) {
		log_error("irtoy: could not enable hardware flow");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}
	dev = irtoy_hw_init(drv.fd);
	if (dev == NULL) {
		log_error("irtoy: No USB Irtoy device found at %s",
			  drv.device);
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}
	log_trace("Version hw %d, sw %d, protocol %d",
		  dev->hwVersion, dev->swVersion, dev->protoVersion);
	if (dev->swVersion < IRTOY_MINFWVERSION) {
		log_error("irtoy: Need firmware V%02d or higher, " \
			  "this firmware: %02d",
			  IRTOY_MINFWVERSION,
			  dev->swVersion);
		free(dev);
		dev = NULL;
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}
	rec_buffer_init();
	send_buffer_init();
	setPin(openPin, 1);
	setPin(sendingPin, 0);
	setPin(receivePin, 1);

	return 1;
}

/* dev.device is const *, so I do not dare to write to it, make my own version */
static char _devname[20];

static int init(void)
{
	int i;
	int found;
	char tmp[64];
	const char* const MSG_MORE_DEVICES =
		"Additional irtoy device found: %s (ignored)";
	const char* const MSG_FOUND = "irtoy device found on %s";

	log_trace("irtoy: init");
	if (drv.device == NULL) {
		log_error("irtoy: NULL device.");
		return 0;
	}
	if (dev != NULL) {
		log_debug("irtoy: init: irtoy already initialized");
		return 1;
	}
	if (strcmp(drv.device, "auto") != 0)
		return init_device();
	for (found = 0, i = 0; i <= 9; i++) {
		if (found) {
			sprintf(tmp, "/dev/ttyACM%d", i);
			drv.device = tmp;
			if (init_device())
				log_warn(MSG_MORE_DEVICES, tmp);
			drv.device = _devname;
		} else {
			sprintf(_devname, "/dev/ttyACM%d", i);
			drv.device = _devname;
			found = init_device();
			if (found)
				log_info(MSG_FOUND, _devname);
		}
	}
	return found;
}

static int deinit(void)
{
	log_trace("irtoy: deinit");

	// IMPORTANT do not remove this reset. it is vital to return the
	// irtoy to IRMAN mode.
	// If we leave the irtoy in sample mode while no-one has the
	// tty open, the linux cdc_acm driver will fail on the next open.
	// This is apparently due to data being sent while the tty is not
	// open and fairly well known
	// (google for "tty_port_close_start: tty->count = 1 port count = 0")
	// IRMAN mode will wait until a signal is actually read before
	// sending the next one, while sample mode will keep streaming
	// (and under fluorescent light it WILL stream..)
	// triggering the problem
	if (dev != NULL) {
		setPin(openPin, 0);
		setPin(sendingPin, 0);
		setPin(receivePin, 0);
		irtoy_reset(dev);
		free(dev);
		dev = NULL;
	}

	close(drv.fd);
	drv.fd = -1;
	tty_delete_lock();
	return 1;
}


static char* receive(struct ir_remote* remotes)
{
	log_trace("irtoy_raw_rec");
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}


static int irtoy_send_double_buffered(unsigned char* signals, int length)
{
	int numToXmit = length;
	int numThisTime;
	int res;
	unsigned char irToyBufLen;
	unsigned char* txPtr;
	unsigned char reply[16];
	int irtoyXmit;

	res = write(dev->fd, IRTOY_COMMAND_TXSTART, sizeof(IRTOY_COMMAND_TXSTART));

	if (res != sizeof(IRTOY_COMMAND_TXSTART)) {
		log_error("irtoy_send: couldn't write command");
		return 0;
	}

	res = read_with_timeout(dev->fd, &irToyBufLen, 1, IRTOY_TIMEOUT_READYFORDATA);

	if (res != 1) {
		log_error("irtoy_send: couldn't read command result");
		return -1;
	}

	log_trace("irtoy ready for %d bytes", irToyBufLen);

	txPtr = signals;

	while (numToXmit) {
		numThisTime = (numToXmit < irToyBufLen) ? numToXmit : irToyBufLen;
		res = write(dev->fd, txPtr, numThisTime);

		if (res != numThisTime) {
			log_error("irtoy_send: couldn't write command");
			return 0;
		}

		txPtr += numThisTime;
		numToXmit -= numThisTime;


		res = read_with_timeout(dev->fd, &irToyBufLen, 1, IRTOY_TIMEOUT_READYFORDATA);

		if (res != 1) {
			log_error("irtoy_send: couldn't read command result");
			return -1;
		}

		log_trace("irtoy ready for %d bytes", irToyBufLen);
	}


	res = read_with_timeout(dev->fd, reply, IRTOY_LEN_XMITRES, IRTOY_TIMEOUT_READYFORDATA);

	if (res != IRTOY_LEN_XMITRES) {
		log_error("irtoy_send: couldn't read command result");
		return -1;
	}

	log_trace("%c %02X %02X %c\n", reply[0], reply[1], reply[2], reply[3]);

	if (reply[0] != IRTOY_REPLY_XMITCOUNT) {
		log_error("irtoy_send: invalid byte count indicator received: %02X", reply[0]);
		return 0;
	}

	irtoyXmit = (reply[1] << 8) | reply[2];
	if (length != irtoyXmit) {
		log_error("irtoy_send: incorrect byte count received: %d expected: %d", irtoyXmit, length);
		return 0;
	}

	if (reply[3] != IRTOY_REPLY_XMITSUCCESS) {
		log_error("irtoy_send: received error status %02X", reply[3]);
		return 0;
	}

	return 1;
}


static int send(struct ir_remote* remote, struct ir_ncode* code)
{
	int length;
	const lirc_t* signals;

	int numToXmit;
	int i;
	lirc_t val;
	int res;

	log_trace("irtoy: send");

	if (dev == NULL) {
		log_error("irtoy: send: irtoy not initialized");
		return 0;
	}

	if (!send_buffer_put(remote, code))
		return 0;

	length = send_buffer_length();
	signals = send_buffer_data();

	for (i = 0; i < length; i++) {
		val = (lirc_t)(((double)signals[i]) / IRTOY_UNIT);
		rawSB[2 * i] = val >> 8;
		rawSB[2 * i + 1] = val & 0xFF;
	}

	rawSB[2 * length] = 0xFF;
	rawSB[2 * length + 1] = 0xFF;

	numToXmit = 2 * length + 2;
	setPin(sendingPin, 1);
	res = irtoy_send_double_buffered(rawSB, numToXmit);
	setPin(sendingPin, 0);
	return res;
}

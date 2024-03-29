/*****************************************************************************
** hw_irlink.c **************************************************************
*****************************************************************************
* Routines for the IRLink VS Infrared receiver
*
* Copyright (C) 2007 Maxim Muratov <mumg at mail.ru>
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
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyUSB0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <termios.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

//Forwards:
static int irlink_init(void);
static int irlink_deinit(void);
static char* irlink_rec(struct ir_remote* remotes);
static lirc_t irlink_readdata(lirc_t timeout);
static int drvctl_func(unsigned int cmd, void* arg);


const struct driver hw_irlink = {
	.name		= "irlink",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_MODE2,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= irlink_init,
	.deinit_func	= irlink_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= irlink_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= drvctl_func,
	.readdata	= irlink_readdata,
	.api_version	= 3,
	.driver_version = "0.10.2",
	.info		= "No info available",
	.device_hint    = "drvctl",
};

const struct driver* hardwares[] = { &hw_irlink, (const struct driver*)NULL };

typedef enum {
	BLOCK,
	NON_BLOCK
} IRLINK_READ_MODE_T;

#define IS_IRLINK_LONG_PULSE(code) (code == 0xFF)
#define IS_IRLINK_LONG_PAUSE(code) (code == 0xFE)

#define IS_IRLINK_PULSE(code) (code & 0x01)

#define IS_IRLINK_TIMESCALE_3600(code)  ((code & 0x80) > 0)
#define IS_IRLINK_TIMESCALE_14400(code) ((code & 0x80) == 0)

#define IRLINK_DETECT_CMD 0x81

#define IRLINK_PERIOD(value, timescale) \
	(((1000000 * ((unsigned int)value)) / timescale) & PULSE_MASK)

static const logchannel_t logchannel = LOG_DRIVER;

static int is_long_pause = 0;
static int is_long_pulse = 0;
static struct timeval last_time = { 0 };

static unsigned char pulse = 1;
static lirc_t last_code = 0;

static int drvctl_func(unsigned int cmd, void* arg)
{
	switch (cmd) {
	case DRVCTL_GET_DEVICES:
		return drv_enum_glob((glob_t*) arg, "/dev/ttyUSB*");
	case DRVCTL_FREE_DEVICES:
		drv_enum_free((glob_t*) arg);
		return 0;
	default:
		return DRV_ERR_NOT_IMPLEMENTED;
	}
}


static int irlink_close(const int port)
{
	if (port != -1) {
		tty_delete_lock();
		close(port);
		return 0;
	}
	return -1;
}

static int irlink_open(const char* portName)
{
	int port = -1;

	if (!tty_create_lock((char*)portName)) {
		log_error("could not create lock files");
		return -1;
	}
	port = open(portName, O_RDWR | O_NOCTTY | O_NDELAY);
	if (port < 0) {
		log_error("could not open %s", drv.device);
		tty_delete_lock();
		return -1;
	}
	if (tty_reset(port) < 0 || tty_setbaud(port, 115200) < 0 || tty_setcsize(port, 8) < 0
	    || tty_setrtscts(port, 0) < 0) {
		irlink_close(port);
		port = -1;
	}
	return port;
}

static int irlink_read(const int port, unsigned char* buffer, int bufferSize)
{
	if (port == -1)
		return -1;
	return read(port, buffer, bufferSize);
}

static int irlink_write(const int port, unsigned char* buffer, int bufferSize)
{
	if (port == -1)
		return -1;
	return write(port, buffer, bufferSize);
}

static void irlink_read_flush(const int port)
{
	struct pollfd pfd = {.fd = port, .events = POLLIN, .revents = 0};
	lirc_t data = 0;

	/* FIXME: This loop makes no sense. */
	for (;; ) {
		if (curl_poll(&pfd, 1, 0) > 0) {
			if (read(port, &data, sizeof(data)) <= 0)
				break;
		} else {
			break;
		}
	}
}

static int irlink_detect(const int port)
{
	unsigned char detect_cmd[] = { IRLINK_DETECT_CMD };

	if (port == -1)
		return -1;
	irlink_read_flush(port);
	if (irlink_write(port, detect_cmd, sizeof(detect_cmd)) == sizeof(detect_cmd)) {
		unsigned char detect_response = 0;

		if (waitfordata(500000)
		    && irlink_read(port, &detect_response, sizeof(detect_response)) == sizeof(detect_response)) {
			if (detect_response == IRLINK_DETECT_CMD)
				return 0;
		}
	}
	return -1;
}

char* irlink_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

lirc_t irlink_readdata(lirc_t timeout)
{
	lirc_t data = 0;
	unsigned char rd_value = 0;
	struct timeval start_time = { 0 };

	gettimeofday(&start_time, NULL);
	lirc_t time_delta = 0;

	for (;; ) {
		if (last_code != 0) {
			data = last_code;
			last_code = 0;
			break;
		}
		if (timeout < time_delta) {
			log_error("timeout < time_delta");
			break;
		}
		if (!waitfordata(timeout - time_delta))
			break;
		if (irlink_read(drv.fd, &rd_value, sizeof(rd_value)) == sizeof(rd_value)) {
			if (IS_IRLINK_LONG_PULSE(rd_value) || IS_IRLINK_LONG_PAUSE(rd_value)) {
				struct timeval diff_time = { 0 };

				is_long_pulse = IS_IRLINK_LONG_PULSE(rd_value);
				is_long_pause = IS_IRLINK_LONG_PAUSE(rd_value);
				gettimeofday(&last_time, NULL);
				timersub(&last_time, &start_time, &diff_time);
				time_delta = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
				continue;
			} else {
				lirc_t* code_ptr = &data;

				if (is_long_pulse != 0 || is_long_pause != 0) {
					struct timeval curr_time;
					struct timeval diff_time;

					gettimeofday(&curr_time, NULL);
					timersub(&curr_time, &last_time, &diff_time);

					if (diff_time.tv_sec >= 16)
						data = PULSE_MASK;
					else
						data = diff_time.tv_sec * 1000000 + diff_time.tv_usec;
					if (is_long_pause) {
						is_long_pause = 0;
						pulse = 1;
						data &= ~PULSE_BIT;
					}
					if (is_long_pulse) {
						pulse = 0;
						is_long_pulse = 0;
						data |= PULSE_BIT;
					}
					code_ptr = &last_code;
				}
				if (IS_IRLINK_TIMESCALE_3600(rd_value)) {
					rd_value = (rd_value & 0x7F) >> 1;
					*code_ptr = IRLINK_PERIOD(rd_value, 3600);
				} else if (IS_IRLINK_TIMESCALE_14400(rd_value)) {
					rd_value = (rd_value & 0x7F) >> 1;
					*code_ptr = IRLINK_PERIOD(rd_value, 14400);
				}
				if (pulse == 0)
					*code_ptr &= ~PULSE_BIT;
				else
					*code_ptr |= PULSE_BIT;
				pulse = !pulse;
				break;
			}
		} else {
			log_error("error reading from %s", drv.device);
			log_perror_err(NULL);
			irlink_deinit();
		}
	}
	return data;
}

int irlink_init(void)
{
	drv.fd = irlink_open(drv.device);
	if (drv.fd < 0) {
		log_error("Could not open the '%s' device", drv.device);
	} else {
		if (irlink_detect(drv.fd) == 0) {
			return 1;
		}
		log_error("Failed to detect IRLink on '%s' device", drv.device);
		irlink_deinit();
	}
	return 0;
}

int irlink_deinit(void)
{
	if (drv.fd != -1)
		irlink_close(drv.fd);
	drv.fd = -1;
	return 1;
}

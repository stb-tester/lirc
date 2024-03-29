/****************************************************************************
** hw_uirt2_raw.c **********************************************************
****************************************************************************
*
* Routines for UIRT2 receiver/transmitter.
* Receiving using the raw mode and transmitting using struc or raw mode,
* depending on code length.
*
* Copyright (C) 2003 Mikael Magnusson <mikma@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

#include "uirt2_common.h"

#define NUMBYTES 6

static const logchannel_t logchannel = LOG_DRIVER;

static uirt2_t* dev;
static lirc_t rec_buf[200];
static int rec_rptr;
static int rec_wptr;
static int rec_size;

/* exported functions  */
static int uirt2_raw_init(void);
static int uirt2_raw_deinit(void);
static int uirt2_send(struct ir_remote* remote, struct ir_ncode* code);
static char* uirt2_raw_rec(struct ir_remote* remotes);
static int uirt2_raw_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static lirc_t uirt2_raw_readdata(lirc_t timeout);

/* forwards */
static int uirt2_send_mode2_raw(uirt2_t* dev, struct ir_remote* remote, const lirc_t* buf, int length);
static int uirt2_send_mode2_struct1(uirt2_t* dev, struct ir_remote* remote, const lirc_t* buf, int length);

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyS0"
#endif

const struct driver hw_uirt2_raw = {
	.name		= "uirt2_raw",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= uirt2_raw_init,
	.deinit_func	= uirt2_raw_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= uirt2_send,
	.rec_func	= uirt2_raw_rec,
	.decode_func	= uirt2_raw_decode,
	.drvctl_func	= NULL,
	.readdata	= uirt2_raw_readdata,
	.api_version	= 3,
	.driver_version = "0.10.2",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver hw_usb_uirt_raw = {
	.name		= "usb_uirt_raw",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= uirt2_raw_init,
	.deinit_func	= uirt2_raw_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= uirt2_send,
	.rec_func	= uirt2_raw_rec,
	.decode_func	= uirt2_raw_decode,
	.drvctl_func	= NULL,
	.readdata	= uirt2_raw_readdata,
	.api_version	= 3,
	.driver_version = "0.10.2",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_usb_uirt_raw, &hw_uirt2_raw, NULL };

/*
 * queue
 */
static int queue_put(lirc_t data)
{
	int next = (rec_wptr + 1) % rec_size;

	log_trace2("queue_put: %d", data);

	if (next != rec_rptr) {
		rec_buf[rec_wptr] = data;
		rec_wptr = next;
		return 0;
	}
	log_error("uirt2_raw: queue full");
	return -1;
}

static int queue_get(lirc_t* pdata)
{
	if (rec_wptr != rec_rptr) {
		*pdata = rec_buf[rec_rptr];
		rec_rptr = (rec_rptr + 1) % rec_size;
		log_trace2("queue_get: %d", *pdata);

		return 0;
	}
	log_error("uirt2_raw: queue empty");
	return -1;
}

static int queue_is_empty(void)
{
	return rec_wptr == rec_rptr;
}

static void queue_clear(void)
{
	rec_rptr = 0;
	rec_wptr = 0;
}

static int uirt2_raw_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	int res;

	log_trace("uirt2_raw_decode: enter");

	res = receive_decode(remote, ctx);

	log_trace("uirt2_raw_decode: %d", res);

	return res;
}

static lirc_t uirt2_raw_readdata(lirc_t timeout)
{
	lirc_t data = 0;

	if (queue_is_empty()) {
		lirc_t data = uirt2_read_raw(dev, timeout);

		if (!data) {
			log_trace("uirt2_raw_readdata failed");
			return 0;
		}

		queue_put(data);
	}

	queue_get(&data);

	log_trace("uirt2_raw_readdata %d %d", !!(data & PULSE_BIT), data & PULSE_MASK);

	return data;
}

static int uirt2_raw_init(void)
{
	int version;

	if (!tty_create_lock(drv.device)) {
		log_error("uirt2_raw: could not create lock files");
		return 0;
	}

	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("uirt2_raw: could not open %s", drv.device);
		tty_delete_lock();
		return 0;
	}

	if (!tty_reset(drv.fd)) {
		log_error("uirt2_raw: could not reset tty");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}

	/* Wait for UIRT device to power up */
	usleep(100 * 1000);

	if (!tty_setbaud(drv.fd, 115200)) {
		log_error("uirt2_raw: could not set baud rate");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}

	if (!tty_setcsize(drv.fd, 8)) {
		log_error("uirt2_raw: could not set csize");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}

	if (!tty_setrtscts(drv.fd, 1)) {
		log_error("uirt2_raw: could not enable hardware flow");
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}

	dev = uirt2_init(drv.fd);
	if (dev == NULL) {
		log_error("uirt2_raw: No UIRT2 device found at %s", drv.device);
		close(drv.fd);
		tty_delete_lock();
		return 0;
	}

	if (uirt2_setmoderaw(dev) < 0) {
		log_error("uirt2_raw: could not set raw mode");
		uirt2_raw_deinit();
		return 0;
	}

	if (uirt2_getversion(dev, &version) < 0) {
		uirt2_raw_deinit();
		return 0;
	}
	if (version >= 0x0905) {
		if (!tty_setdtr(drv.fd, 0)) {
			log_error("uirt2_raw: could not set DTR");
			uirt2_raw_deinit();
			return 0;
		}
	}

	rec_buffer_init();
	send_buffer_init();

	rec_rptr = 0;
	rec_wptr = 0;
	rec_size = sizeof(rec_buf) / sizeof(rec_buf[0]);

	return 1;
}

static int uirt2_raw_deinit(void)
{
	int version;

	if (uirt2_setmodeuir(dev) < 0)
		log_warn("uirt2_raw: could not set uir mode");
	if (uirt2_getversion(dev, &version) >= 0 && version >= 0x0905)
		tty_setdtr(drv.fd, 1);
	uirt2_uninit(dev);
	dev = NULL;
	close(drv.fd);
	drv.fd = -1;
	tty_delete_lock();
	return 1;
}

static char* uirt2_raw_rec(struct ir_remote* remotes)
{
	log_trace("uirt2_raw_rec");
	log_trace("uirt2_raw_rec: %p", remotes);

	if (!rec_buffer_clear())
		return NULL;

	if (remotes) {
		char* res;

		res = decode_all(remotes);
		return res;
	}
	lirc_t data;

	queue_clear();
	data = uirt2_read_raw(dev, 1);
	if (data)
		queue_put(data);
	return NULL;
}

static int uirt2_send(struct ir_remote* remote, struct ir_ncode* code)
{
	int i, length;
	unsigned long delay;
	const lirc_t* signals;
	int res = 0;

	if (!send_buffer_put(remote, code))
		return 0;

	length = send_buffer_length();
	signals = send_buffer_data();

	if (length <= 0 || signals == NULL) {
		log_trace("nothing to send");
		return 0;
	}


	log_trace("Trying REMSTRUC1 transmission");
	res = uirt2_send_mode2_struct1(dev, remote, signals, length);
	if (!res && (length < 48)) {
		log_trace("Using RAW transission");
		res = uirt2_send_mode2_raw(dev, remote, signals, length);
	}

	if (!res) {
		log_error("uirt2_send: remote not supported");
	} else {
		log_trace("uirt2_send: succeeded");
	}
	/*
	 * Some devices send the sequence in the background.  Wait for
	 * the sequence to complete before returning in order to avoid
	 * disturbing DTR which is used by certain hardware revisions
	 * to enable the builtin emitter.  We wait 1.1 times the expected
	 * time in order to handle any differences between the device and
	 * our clock.
	 */
	delay = remote->min_remaining_gap;
	for (i = 0; i < length; i++)
		delay += signals[i];
	delay = (delay * 11) / 10;
	usleep(delay);

	return res;
}

static int uirt2_send_mode2_raw(uirt2_t*		dev,
				struct ir_remote*	remote,
				const lirc_t*		buf,
				int			length)
{
	byte_t tmp[64];
	int i, dest;
	int ir_length = 0;
	int res;
	int repeats = 1;

	log_trace("uirt2_send_mode2_raw %d %p", length, buf);

	tmp[0] = 0;
	tmp[1] = 0;

	for (i = 0, dest = 2; i < length; i++) {
		int val = buf[i] / UIRT2_UNIT;

		while (val > 0) {
			if (val > UCHAR_MAX) {
				tmp[dest++] = UCHAR_MAX - 1;
				tmp[dest++] = 1;        /* 0 won't work */
				val -= UCHAR_MAX;
				length += 2;
			} else {
				tmp[dest++] = val;
				val = 0;
			}
			if (dest - 2 > 48) {
				log_error("uirt2_raw: too long RAW transmission %d > 48", dest - 2);
				return 0;
			}
		}
		ir_length += buf[i];
	}

	tmp[dest++] = uirt2_calc_freq(remote->freq) + (repeats & 0x1f);

	res = uirt2_send_raw(dev, tmp, dest);

	if (!res)
		return 0;

	log_trace("uirt2_send_mode2_raw exit");
	return 1;
}

static void set_data_bit(byte_t* dest, int offset, int bit)
{
	int i = offset / 8;
	int j = offset % 8;
	int mask = 1 << j;

	byte_t src = dest[i];
	byte_t dst;

	if (bit)
		dst = src | mask;
	else
		dst = src & ~mask;

	dest[i] = dst;
}

static int calc_data_bit(struct ir_remote* remote, int table[], int table_len, int signal, int tUnit)
{
	int i;

	for (i = 0; i < table_len; i++) {
		if (table[i] == 0) {
			table[i] = signal / tUnit;

			log_trace1("table[%d] = %d\n", i, table[i]);

			return i;
		}

		if (expect(remote, signal, table[i] * tUnit)) {
			log_trace1("expect %d, table[%d] = %d\n", signal / tUnit, i, table[i]);
			return i;
		}
	}

	log_trace1("Couldn't find %d\n", signal / tUnit);

	return -1;
}

static int uirt2_send_mode2_struct1(uirt2_t*		dev,
				    struct ir_remote*	remote,
				    const lirc_t*	buf,
				    int			length)
{
	const int TABLE_LEN = 2;
	remstruct1_data_t rem;
	int res;
	int table[2][TABLE_LEN];
	int bits = 0;
	int i, j;
	int tUnit;
	int freq;
	int bFrequency;
	int version;
	int repeats = 1;

	memset(&rem, 0, sizeof(rem));

	memset(table[0], 0, sizeof(table[0]));
	memset(table[1], 0, sizeof(table[1]));

	res = uirt2_getversion(dev, &version);
	if (res < 0)
		return res;
	log_info("uirt2_raw: UIRT version %04x", version);
	freq = remote->freq;
	if (freq == 0)
		freq = DEFAULT_FREQ;
	if (version >= 0x0905) {
		if (((5000000 / freq) + 1) / 2 >= 0x80)
			bFrequency = 0x80;
		else
			bFrequency = ((5000000 / freq) + 1) / 2;
		tUnit = (bFrequency * 100) / 125;
	} else {
		tUnit = UIRT2_UNIT;
	}
	for (i = 0; i < length; i++) {
		int bit;
		int len = buf[i] / tUnit;

		if (len > UCHAR_MAX) {
			log_trace("signal too long for transmission %lu", (uint32_t)buf[i]);
			return 0;
		}
		if (i == 0) {
			rem.bHdr1 = len;
			continue;
		} else if (i == 1) {
			rem.bHdr0 = len;
			continue;
		}

		bit = calc_data_bit(remote, table[i % 2], TABLE_LEN, buf[i], tUnit);

		if (bit < 0) {
			int part_length = i + 1;

			/* is this a repeated signal sequence? */
			if (!(i % 2 /* space */ && buf[i] == remote->min_remaining_gap))
				return 0;

			if ((length + 1) % part_length != 0)
				return 0;

			repeats = (length + 1) / part_length;

			for (j = 1; j < repeats; j++) {
				if (memcmp
					    (&buf[0], &buf[j * part_length],
					    (j + 1 == repeats ? part_length - 1 : part_length) * sizeof(*buf)) != 0)
					return 0;
			}
			break;
		}

		if (i - 2 > UIRT2_MAX_BITS) {
			log_error("uirt2_raw: UIRT tried to send %d bits, max is %d", length - 2,
				  UIRT2_MAX_BITS);

			return 0;
		}

		set_data_bit(rem.bDatBits, i - 2, bit);
		bits++;
	}

	log_trace1("bits %d", bits);

	rem.bISDlyHi = remote->min_remaining_gap / tUnit / 256;
	rem.bISDlyLo = (remote->min_remaining_gap / tUnit) & 255;
	rem.bBits = bits;
	rem.bOff0 = table[1][0];
	rem.bOff1 = table[1][1];
	rem.bOn0 = table[0][0];
	rem.bOn1 = table[0][1];

	res = uirt2_send_struct1(dev, freq, repeats, &rem);

	return res;
}

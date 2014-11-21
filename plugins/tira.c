/*****************************************************************************
 ** tira.c ****************************************************************
 *****************************************************************************
 * Routines for the HomeElectronics TIRA-2 USB dongle.
 *
 * Serial protocol described at:
 *    http://www.home-electro.com/Download/Protocol2.pdf
 *
 * Copyright (C) 2003 Gregory McLean <gregm@gxsnmp.org>
 *  modified for
 *  IRA support,
 *  transmit feature,
 *  receive in pulse/space mode feature
 *  by Arnold Pakolitz <spud28@gmail.com>
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
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <termios.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

int tira_decode(struct ir_remote *remote, struct decode_ctx_t* ctx);
int tira_init(void);
int tira_deinit(void);
char *tira_rec(struct ir_remote *remotes);
char *tira_rec_mode2(struct ir_remote *remotes);
static int tira_send(struct ir_remote *remote, struct ir_ncode *code);
lirc_t tira_readdata(lirc_t timeout);

const char failwrite[] = "failed writing to device";
const char strreconly[] = "receive";
const char strsendrec[] = "send / receive";
const char mode_sixbytes[] = "6 bytes mode";
const char mode_timing[] = "timing mode";
static unsigned char deviceflags = 0;

int pipe_fd[2] = { -1, -1 };

static unsigned char device_type = 0;	//'t' for tira,'i' for ira
static pid_t child_pid = -1;
static struct timeval start, end, last;
static unsigned char b[6];
static unsigned char pulse_space;	//1=pulse

static ir_code code;

#define CODE_LENGTH 64
const struct driver hw_tira = {
	.name		=	"tira",
	.device		=	LIRC_IRTTY,
	.features	=	LIRC_CAN_REC_LIRCCODE | LIRC_CAN_SEND_PULSE,
	.send_mode	=	LIRC_MODE_PULSE,
	.rec_mode	=	LIRC_MODE_LIRCCODE,
	.code_length	=	CODE_LENGTH,
	.init_func	=	tira_init,
	.deinit_func	=	tira_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	tira_send,
	.rec_func	=	tira_rec,
	.decode_func	=	tira_decode,
	.drvctl_func	=	NULL,
	.readdata	=	NULL,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=	"No info available"
};

const struct driver hw_tira_raw = {
	.name		=	"tira_raw",
	.device		=	LIRC_IRTTY,
	.features	=	LIRC_CAN_REC_MODE2,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_MODE2,
	.code_length	=	CODE_LENGTH,
	.init_func	=	tira_init,
	.deinit_func	=	tira_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	NULL,	/* Cannot transmit in timing mode */
	.rec_func	=	tira_rec_mode2,
	.decode_func	=	tira_decode,
	.drvctl_func	=	NULL,
	.readdata	=	tira_readdata,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=	"No info available"
};
const struct driver* hardwares[] = { &hw_tira, &hw_tira_raw, NULL };


int tira_decode(struct ir_remote *remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 0, 0, CODE_LENGTH, code, 0, 0)) {
		return 0;
	}

	map_gap(remote, ctx, &start, &last, 0);

	return 1;
}

char response[64 + 1];

void displayonline(void)
{
	const char *dflags;
	const char *dmode;

	dflags = strreconly;
	if (deviceflags & 1)
		dflags = strsendrec;
	if (drv.rec_mode == LIRC_MODE_LIRCCODE)
		dmode = mode_sixbytes;
	else
		dmode = mode_timing;
	logprintf(LIRC_INFO, "device online, ready to %s remote codes(%s)", dflags, dmode);
}

int tira_setup_sixbytes(void)
{
	int i;

	logprintf(LIRC_INFO, "Switching to 6bytes mode");
	if (write(drv.fd, "IR", 2) != 2) {
		logprintf(LIRC_ERROR, "failed switching device into six byte mode");
		return 0;
	}
	/* wait for the chars to be written */
	usleep(2 * (100 * 1000));

	i = read(drv.fd, response, 2);
	if (i != 2) {
		logprintf(LIRC_ERROR, "failed reading response to six byte mode command");
		return 0;
	}
	if (strncmp(response, "OK", 2) != 0)
		return 0;
	displayonline();
	return 1;
}

//This process reads data from the device and forwards to the hw pipe
//as PULSE/SPACE data
int child_process(int pipe_w, int oldprotocol)
{
	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	unsigned char tirabuffer[64];
	int tirabuflen = 0;
	int readsize, tmp;
	lirc_t data, tdata;
	fd_set read_set;
	struct timeval tv, trailtime, currtime;
	__u32 eusec;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	FD_ZERO(&read_set);

	trailtime.tv_sec = 0;
	trailtime.tv_usec = 0;

	while (1) {
		FD_SET(drv.fd, &read_set);
		tmp = select(drv.fd + 1, &read_set, NULL, NULL, &tv);

		if (tmp == 0)
			continue;
		if (tmp < 0) {
			logprintf(LIRC_ERROR, "Error select()");
			return 0;
		}

		if (!FD_ISSET(drv.fd, &read_set))
			continue;
		readsize = read(drv.fd, &tirabuffer[tirabuflen], sizeof(tirabuffer) - tirabuflen);
		if (readsize <= 0) {
			logprintf(LIRC_ERROR, "Error reading from Tira");
			logperror(LIRC_ERROR, NULL);
			return 0;
		}
		if (readsize == 0)
			continue;
		tirabuflen += readsize;

		tmp = 0;
		while (tmp < tirabuflen - 1) {
			data = tirabuffer[tmp];
			data <<= 8;
			data += tirabuffer[tmp + 1];
			if (!oldprotocol)
				data <<= 3;
			else
				data <<= 5;
			if (data == 0) {
				if (tmp > tirabuflen - 4)
					break;	//we have to receive more data
				if (tirabuffer[tmp + 3] != 0xB2) {
					logprintf(LIRC_ERROR, "Tira error 00 00 xx B2 trailing : missing 0xB2");
					return 0;
				}
				if ((trailtime.tv_sec == 0) && (trailtime.tv_usec == 0))
					gettimeofday(&trailtime, NULL);
				if (tmp > tirabuflen - 6)
					break;	//we have to receive more data
				tmp += 4;
				continue;
			}
			tmp += 2;

			if ((trailtime.tv_sec != 0) || (trailtime.tv_usec != 0)) {
				gettimeofday(&currtime, NULL);
				if (trailtime.tv_usec > currtime.tv_usec) {
					currtime.tv_usec += 1000000;
					currtime.tv_sec--;
				}

				eusec = time_elapsed(&trailtime, &currtime);
				if (eusec > LIRC_VALUE_MASK)
					eusec = LIRC_VALUE_MASK;

				trailtime.tv_sec = 0;
				trailtime.tv_usec = 0;
				if (eusec > data) {
					pulse_space = 1;
					tdata = LIRC_SPACE(eusec);
					if (write(pipe_w, &tdata, sizeof(tdata)) != sizeof(tdata)) {
						logprintf(LIRC_ERROR, "Error writing pipe");
						return 0;
					}

				}
			}

			data = pulse_space ? LIRC_PULSE(data) : LIRC_SPACE(data);

			pulse_space = 1 - pulse_space;

			if (write(pipe_w, &data, sizeof(data)) != sizeof(data)) {
				logprintf(LIRC_ERROR, "Error writing pipe");
				return 0;
			}
		}

		//Scroll buffer to next position
		if (tmp > 0) {
			memmove(tirabuffer, tirabuffer + tmp, tirabuflen - tmp);
			tirabuflen -= tmp;
		}
	}
}

int tira_setup_timing(int oldprotocol)
{
	long fd_flags;
	int i;

	if (oldprotocol)
		if (!tty_setbaud(drv.fd, 57600))
			return 0;

	logprintf(LIRC_INFO, "Switching to timing mode");
	if (!oldprotocol) {
		if (write(drv.fd, "IC\0\0", 4) != 4) {
			logprintf(LIRC_ERROR, "failed switching device into timing mode");
			return 0;
		}
		/* wait for the chars to be written */
		usleep(2 * (100 * 1000));

		i = read(drv.fd, response, 3);
		if (i != 3) {
			logprintf(LIRC_ERROR, "failed reading response to timing mode command");
			return 0;
		}
		if (strncmp(response, "OIC", 3) != 0)
			return 0;
	}
	pulse_space = 1;	//pulse
	/* Allocate a pipe for lircd to read from */
	if (pipe(pipe_fd) == -1) {
		logperror(LIRC_ERROR, "unable to create pipe");
		goto fail;
	}

	fd_flags = fcntl(pipe_fd[0], F_GETFL);
	if (fcntl(pipe_fd[0], F_SETFL, fd_flags | O_NONBLOCK) == -1) {
		logperror(LIRC_ERROR, "can't set pipe to non-blocking");
		goto fail;
	}

	/* Spawn the child process */

	child_pid = fork();
	if (child_pid == -1) {
		logperror(LIRC_ERROR, "unable to fork child process");
		goto fail;
	} else if (child_pid == 0) {
		close(pipe_fd[0]);
		child_process(pipe_fd[1], oldprotocol);
		close(pipe_fd[1]);
		_exit(0);
	} else {
		/* parent reads from pipe */
		close(drv.fd);
		drv.fd = pipe_fd[0];
	}
	close(pipe_fd[1]);
	displayonline();
	return (1);

fail:
	if (pipe_fd[0] != -1) {
		close(pipe_fd[0]);
		close(pipe_fd[1]);
	}
	return (0);

}

int tira_setup(void)
{
	int ptr;
	/* Clear the port of any random data */
	while (read(drv.fd, &ptr, 1) >= 0) ;

	/* Start off with the IP command. This was initially used to
	   switch to timing mode on the Tira-1. The Tira-2 also
	   supports this mode, however it does not switch the Tira-2
	   into timing mode.
	 */
	if (write(drv.fd, "IP", 2) != 2) {
		logprintf(LIRC_ERROR, failwrite);
		return 0;
	}
	/* Wait till the chars are written, should use tcdrain but
	   that don't seem to work... *shrug*
	 */
	usleep(2 * (100 * 1000));
	chk_read(drv.fd, response, 3);

	if (strncmp(response, "OIP", 3) == 0) {
		chk_read(drv.fd, &ptr, 1);	/* read the calibration value */
		chk_read(drv.fd, &ptr, 1);	/* read the version word */
		/* Bits 4:7 in the version word set to one indicates a
		   Tira-2 */
		deviceflags = ptr & 0x0f;
		if (ptr & 0xF0) {
			logprintf(LIRC_INFO, "Tira-2 detected");
			/* Lets get the firmware version */
			chk_write(drv.fd, "IV", 2);
			usleep(2 * (100 * 1000));
			memset(response, 0, sizeof(response));
			chk_read(drv.fd, response, sizeof(response) - 1);
			logprintf(LIRC_INFO, "firmware version %s", response);
		} else {
			logprintf(LIRC_INFO, "Ira/Tira-1 detected");
		}

		/* According to the docs we can do some bit work here
		   and figure out what the device supports from the
		   version word retrived.

		   At this point we have a Device of some sort. Lets
		   kick it into "Six bytes or Timing" mode.
		 */
		if (drv.rec_mode == LIRC_MODE_LIRCCODE)
			return (tira_setup_sixbytes());
		if (drv.rec_mode == LIRC_MODE_MODE2)
			return (tira_setup_timing(0));

		return 0;	//unknown recmode
	}
	logprintf(LIRC_ERROR, "unexpected response from device");
	return 0;
}

int ira_setup_sixbytes(unsigned char info)
{
	int i;
	if (info != 0)
		logprintf(LIRC_INFO, "Switching to 6bytes mode");
	if (write(drv.fd, "I", 1) != 1) {
		logprintf(LIRC_ERROR, failwrite);
		return 0;
	}
	usleep(200000);
	if (write(drv.fd, "R", 1) != 1) {
		logprintf(LIRC_ERROR, failwrite);
		return 0;
	}

	/* Wait till the chars are written, should use tcdrain but
	   that don't seem to work... *shrug*
	 */
	usleep(100000);
	i = read(drv.fd, response, 2);
	if (i != 2)
		return 0;
	if (strncmp(response, "OK", 2) != 0)
		return 0;
	if (info != 0)
		displayonline();
	return 1;
}

int ira_setup(void)
{
	int i;
	int ptr;
	/* Clear the port of any random data */
	while (read(drv.fd, &ptr, 1) >= 0) 
   		;

	if (ira_setup_sixbytes(0) == 0)
		return 0;

	if (write(drv.fd, "I", 1) != 1) {
		logprintf(LIRC_ERROR, failwrite);
		return 0;
	}
	usleep(200000);
	if (write(drv.fd, "P", 1) != 1) {
		logprintf(LIRC_ERROR, failwrite);
		return 0;
	}

	/* Wait till the chars are written, should use tcdrain but
	   that don't seem to work... *shrug*
	 */
	if (!tty_setbaud(drv.fd, 57600))
		return 0;
	usleep(50000);
	i = read(drv.fd, response, 5);

	if (!tty_setbaud(drv.fd, 9600))
		return 0;

	if (i < 5)
		return 0;
	if (strncmp(response, "OIP", 3) == 0) {
		deviceflags = response[4] & 0x0f;
		if (response[4] & 0xF0) {
			/* Lets get the firmware version */
			if (write(drv.fd, "I", 1) != 1) {
				logprintf(LIRC_ERROR, failwrite);
				return 0;
			}
			usleep(200000);
			if (write(drv.fd, "V", 1) != 1) {
				logprintf(LIRC_ERROR, failwrite);
				return 0;
			}

			usleep(200000);
			memset(response, 0, sizeof(response));
			i = read(drv.fd, response, sizeof(response) - 1);
			logprintf(LIRC_INFO, "Ira %s detected", response);
		} else {
			logprintf(LIRC_INFO, "Ira-1 detected");
		}

		if (drv.rec_mode == LIRC_MODE_LIRCCODE)
			return (ira_setup_sixbytes(1));	//switch back to 6bytes mode
		if (drv.rec_mode == LIRC_MODE_MODE2)
			return (tira_setup_timing(1));
		return 0;	//unknown recmode
	}
	logprintf(LIRC_ERROR, "unexpected response from device");
	return 0;
}

int check_tira(void)
{
	logprintf(LIRC_ERROR, "Searching for Tira");
	if (!tty_reset(drv.fd) || !tty_setbaud(drv.fd, 9600) || !tty_setrtscts(drv.fd, 1))
		return 0;

	usleep(50000);

	return tira_setup();
}

int check_ira(void)
{
	logprintf(LIRC_ERROR, "Searching for Ira");
	if (!tty_reset(drv.fd) || !tty_setbaud(drv.fd, 9600) || !tty_setrtscts(drv.fd, 0) || !tty_setdtr(drv.fd, 1))
		return 0;

	usleep(50000);

	return ira_setup();
}

int tira_init(void)
{
	if (child_pid != -1)
		tira_deinit();

	LOGPRINTF(1, "Tira init");

	if (!tty_create_lock(drv.device)) {
		logprintf(LIRC_ERROR, "could not create lock files");
		return 0;
	}
	if ((drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0) {
		tty_delete_lock();
		logprintf(LIRC_ERROR, "Could not open the '%s' device", drv.device);
		return 0;
	}
	LOGPRINTF(1, "device '%s' opened", drv.device);

	/* We want 9600 8N1 with CTS/RTS handshaking, lets set that
	 * up. The specs state a baud rate of 100000, looking at the
	 * ftdi_sio driver it forces the issue so we can set to what
	 * we would like. And seeing as this is mapped to 9600 under
	 * windows should be a safe bet.
	 */

	/* Determine device type */
	device_type = 0;
	if (check_tira())
		device_type = 't';
	else if (check_ira())
		device_type = 'i';

	const char *device_string;

	switch (device_type) {
	case 't':
		device_string = "Tira";
		break;
	case 'i':
		device_string = "Ira";
		break;
	default:
		device_string = "unknown";
	}
	LOGPRINTF(1, "device type %s", device_string);

	if (device_type == 0) {
		tira_deinit();
		return 0;
	}

	return 1;
}

int tira_deinit(void)
{
	if (child_pid != -1) {
		/* Kill the child process, and wait for it to exit */
		if (kill(child_pid, SIGTERM) == -1) {
			return (0);
		}
		if (waitpid(child_pid, NULL, 0) == 0) {
			return (0);
		}
		child_pid = -1;
	}

	if (drv.fd != -1) {
		close(drv.fd);	/* pipe_fd[0] or actual device */
		drv.fd = -1;
	}
	sleep(1);
	tty_delete_lock();
	return 1;
}

char *tira_rec_mode2(struct ir_remote *remotes)
{
	if (!rec_buffer_clear())
		return (NULL);
	return (decode_all(remotes));
}

char *tira_rec(struct ir_remote *remotes)
{
	char *m;
	int i, x;

	last = end;
	x = 0;
	gettimeofday(&start, NULL);
	for (i = 0; i < 6; i++) {
		if (i > 0) {
			if (!waitfordata(20000)) {
				LOGPRINTF(0, "timeout reading byte %d", i);
				/* likely to be !=6 bytes, so flush. */
				tcflush(drv.fd, TCIFLUSH);
				return NULL;
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			logprintf(LIRC_ERROR, "reading of byte %d failed.", i);
			logperror(LIRC_ERROR, NULL);
			return NULL;
		}
		LOGPRINTF(1, "byte %d: %02x", i, b[i]);
		x++;
	}
	gettimeofday(&end, NULL);
	code = 0;
	for (i = 0; i < x; i++) {
		code |= ((ir_code) b[i]);
		code = code << 8;
	}

	LOGPRINTF(1, " -> %0llx", (__u64) code);

	m = decode_all(remotes);
	return m;
}

static int tira_send(struct ir_remote *remote, struct ir_ncode *code)
{
	int retval = 0;
	unsigned int freq;
	unsigned char *sendtable;

	if ((deviceflags & 1) == 0) {
		logprintf(LIRC_ERROR, "this device cannot send ir signals!");
		return (0);
	}

	if (drv.rec_mode != LIRC_MODE_LIRCCODE) {
		logprintf(LIRC_ERROR, "can't send ir signals in timing mode!");
		return (0);
	}
	/* set the carrier frequency if necessary */
	freq = remote->freq;
	if (freq == 0)
		freq = DEFAULT_FREQ;
	logprintf(LIRC_INFO, "modulation freq %d Hz", freq);
	freq = 2000000 / freq;	/* this will be the clock word */
	if (freq > 255)
		freq = 255;

	if (!send_buffer_put(remote, code))
		return (0);

	int length, i, s;
	const lirc_t *signals;
	char idx;
	int tmp;

	length = send_buffer_length();
	signals = send_buffer_data();

	sendtable = malloc(length);
	if (sendtable == NULL)
		return retval;
	memset(sendtable, -1, length);

	/* Create burst space array for tira */
	int bsa[12];
	memset(&bsa, 0, sizeof(bsa));

	for (i = 0; i < length; i++) {
		idx = -1;
		tmp = signals[i] / 8;
		/* Find signal length in table */
		for (s = 0; s < 12; s++)
			if (bsa[s] == tmp) {
				idx = s;
				break;
			}

		if (idx == -1)
			for (s = 0; s < 12; s++)
				if ((tmp < bsa[s] + (freq / 16)) && (tmp > bsa[s] - (freq / 16))) {
					idx = s;
					break;
				}

		if (idx == -1) {
			/* Add a new entry into bsa table */
			for (s = 0; s < 12; s++) {
				if (bsa[s] == 0) {
					bsa[s] = tmp;
					idx = s;
					break;
				}
			}
		}

		if (idx == -1) {
			logprintf(LIRC_ERROR, "can't send ir signal with more than 12 different timings");
			return retval;
		}

		sendtable[i] = idx;
	}

	tmp = 0;
	for (i = 0; i < length; i += 2) {
		s = sendtable[i] * 16;
		if (i < length - 1)
			s += sendtable[i + 1];
		else
			s += 15;
		sendtable[tmp] = s;
		tmp++;
	}

	unsigned char *wrtbuf;
	wrtbuf = malloc(length + 28);
	if (wrtbuf == NULL)
		return retval;

	wrtbuf[0] = 'I';
	wrtbuf[1] = 'X';
	wrtbuf[2] = freq;
	wrtbuf[3] = 0;		/* reserved */
	for (i = 0; i < 12; i++) {
		wrtbuf[4 + i * 2] = (bsa[i] & 0xFFFF) >> 8;
		wrtbuf[5 + i * 2] = bsa[i] & 0xFF;
	}

	for (i = 0; i < tmp; i++)
		wrtbuf[28 + i] = sendtable[i];
	length = 28 + tmp;

	if (device_type == 'i') {
		i = length;
		if (write(drv.fd, wrtbuf, 1) != 1)
			i = 0;
		if (i != 0) {
			usleep(200000);
			if (write(drv.fd, &wrtbuf[1], length - 1) != length - 1) {
				i = 0;
			}
		}
	} else
		i = write(drv.fd, wrtbuf, length);

	if (i != length)
		logprintf(LIRC_ERROR, failwrite);
	else {
		usleep(200000);
		i = read(drv.fd, wrtbuf, 3);
		if (strncmp((char *)wrtbuf, "OIX", 3) == 0)
			retval = 1;
		else
			logprintf(LIRC_ERROR, "no response from device");
	}

	free(wrtbuf);
	free(sendtable);

	return retval;
}

lirc_t tira_readdata(lirc_t timeout)
{
	lirc_t data = 0;
	int ret;

	if (!waitfordata((long)timeout))
		return 0;

	ret = read(drv.fd, &data, sizeof(data));
	if (ret != sizeof(data)) {
		logprintf(LIRC_ERROR, "error reading from %s", drv.device);
		logperror(LIRC_ERROR, NULL);
		tira_deinit();
		return 0;
	}
	return data;
}

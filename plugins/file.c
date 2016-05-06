/****************************************************************************
** file.c **********************************************************
****************************************************************************
*
*  File output driver which logs sent data to a disk file. The file used is
*  the regular drv.device.
*
*  Also, it supports the following drvctl options:
*    - 'set-input <path>' which makes is read data  from disk file and
*       deliver it as pulses from the remote.
*    - 'send-space <useconds>' which indeed sends a (typically long) space.
*
*  The exported file descriptor drv.fd reflects the input file, not the
*  output one.
*
*  Upon a request to send a LIRC_EOF pulse the driver will terminate the
*  running process using SIGUSR1.
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
#include <config.h>
#endif

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


/* exported functions  */
static int init_func(void);
static int deinit_func(void);
static int send_func(struct ir_remote* remote, struct ir_ncode* code);
static char* receive_func(struct ir_remote* remotes);
static int open_func(const char* path);
static int close_func(void);
static int decode_func(struct ir_remote* remote, struct decode_ctx_t* ctx);
static lirc_t readdata(lirc_t timeout);
static int drvctl_func(unsigned int cmd, void* arg);


const struct driver drv_test = {
	.name		= "file",
	.device		= "testdata.sym",
	.features	= LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= init_func,
	.deinit_func	= deinit_func,
	.open_func	= open_func,
	.close_func	= close_func,
	.send_func	= send_func,
	.rec_func	= receive_func,
	.decode_func	= decode_func,
	.drvctl_func	= drvctl_func,
	.readdata	= readdata,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "See file://" PLUGINDOCS "/file.html",
	.device_hint    = "/tmp/*",
};


const struct driver* hardwares[] = { &drv_test, (const struct driver*)NULL };

// private data

static const logchannel_t logchannel = LOG_DRIVER;

static FILE* infile = NULL;
static int outfile_fd = -1;
static int lineno = 1;
static int at_eof = 0;

static int decode_func(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	int res;

	log_trace("decode: enter");
	res = receive_decode(remote, ctx);
	log_trace("decode: %d", res);
	return res;
}


static lirc_t readdata(lirc_t timeout)
{
	char line[64];
	char what[16];
	int count;
	int data;
	const char* const close_msg =
		"# Closing infile file after %d lines (data still pending...)\n";

	if (infile == NULL || fgets(line, sizeof(line), infile) == NULL) {
		log_trace("No more input, timeout: %d", timeout);
		if (timeout > 0)
			usleep(timeout);
		if (infile != NULL) {
			fclose(infile);
			infile = NULL;
		}
		snprintf(line, sizeof(line), close_msg, lineno);
		chk_write(outfile_fd, line, strlen(line));
		drv.fd = -1;
		at_eof = 1;
		log_debug("Closing infile after  %d lines", lineno);
		lineno = 0;
		return LIRC_EOF | LIRC_MODE2_TIMEOUT | timeout;
	}
	count = sscanf(line, "%15s %d", what, &data);
	if (count != 2)
		return 0;
	data &= PULSE_MASK;
	if (strstr(what, "pulse") != NULL)
		data |= PULSE_BIT;
	lineno += 1;
	return data;
};


static int open_func(const char* device)
{
	if (device == NULL)
		device = drv.device;
	if (device == NULL) {
		log_error("Attempt to open NULL sink file");
		return 0;
	}
	outfile_fd = open(device, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (outfile_fd == -1) {
		log_warn(
			  "Cannot open path %s for write", drv.device);
		return 0;
	}
	send_buffer_init();
	return 1;
}


static int close_func(void)
{
	if (drv.fd == -1)
		return 1;
	if (close(drv.fd) == -1) {
		log_perror_warn("deinit: Cannot close");
		return 0;
	}
	drv.fd = -1;
	return 1;
}


static char* receive_func(struct ir_remote* remotes)
{
	static char* EOF_PACKET = "0000000008000000 00 __EOF lirc";

	if (at_eof) {
		log_trace("file.c: At eof");
		at_eof = 0;
		return EOF_PACKET;
	}
	if (!rec_buffer_clear()) {
		log_trace("file.c: At !rec_buffer_clear");
		if (at_eof) {
			at_eof = 0;
			return EOF_PACKET;
		} else {
			return NULL;
		}
	}
	return decode_all(remotes);
}


static void write_line(const char* type, int duration)
{
	char buffer[32];

	snprintf(buffer, sizeof(buffer), "%s %d\n", type, duration);
	chk_write(outfile_fd, buffer, strlen(buffer));
	if (duration & LIRC_EOF) {
		log_notice("Exiting on input EOF");
		raise(SIGUSR1);
	}
}


static int send_func(struct ir_remote* remote, struct ir_ncode* code)
{
	int i;

	log_trace("file.c: sending, code: %s", code->name);

	if (remote->pzero == 0 && remote->szero == 0
		&& ((remote->flags & RAW_CODES) == 0))
	{
		write_line("code", code->code);
		return 1;
	}
	if (!send_buffer_put(remote, code)) {
		log_debug("file.c: Cannot make send_buffer_put");
		return 0;
	}
	for (i = 0;; ) {
		write_line("pulse", send_buffer_data()[i++]);
		if (i >= send_buffer_length())
			break;
		write_line("space", send_buffer_data()[i++]);
	}
	write_line("space", remote->min_remaining_gap);
	return 1;
}


static int drvctl_func(unsigned int cmd, void* arg)
{
	struct option_t* opt;
	long value;
	char buff[64];
	const char* const open_msg = "# Reading from %s\n";

	switch (cmd) {
	case DRVCTL_SET_OPTION:
		at_eof = 0;    //FIXME! This should be a separate drvctl!
		opt = (struct option_t*)arg;
		if (strcmp(opt->key, "send-space") == 0) {
			value = strtol(opt->value, NULL, 10);
			if (value <= 0 || value > 100000000)
				return DRV_ERR_BAD_OPTION;
			snprintf(buff, sizeof(buff), "space %ld\n", value);
			chk_write(outfile_fd, buff, strlen(buff));
			return 0;
		} else if (strcmp(opt->key, "set-infile") == 0) {
			if (outfile_fd < 0)
				return DRV_ERR_BAD_STATE;
			infile = fopen(opt->value, "r");
			if (infile == NULL)
				return DRV_ERR_BAD_OPTION;
			drv.fd = fileno(infile);
			lineno = 1;
			snprintf(buff, sizeof(buff), open_msg, opt->value);
			chk_write(outfile_fd, buff, strlen(buff));
			return 0;
		} else {
			return DRV_ERR_BAD_OPTION;
		}
	default:
		return DRV_ERR_NOT_IMPLEMENTED;
	}
}


static int init_func(void)
{
	return 1;
};

static int deinit_func(void)
{
	return 1;
};

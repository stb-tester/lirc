/****************************************************************************
** hw_ftdi.c ***************************************************************
****************************************************************************
*
* Mode2 receiver + transmitter using the bitbang mode of an FTDI
* USB-to-serial chip such as the FT232R, with a demodulating IR receiver
* connected to one of the FTDI chip's data pins -- by default, D1 (RXD).
*
* Copyright (C) 2008 Albert Huitsing <albert@huitsing.nl>
* Copyright (C) 2008 Adam Sampson <ats@offog.org>
*
* Inspired by the UDP driver, which is:
* Copyright (C) 2002 Jim Paris <jim@jtan.com>
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined (HAVE_LIBUSB_1_0_LIBUSB_H)
#include <libusb-1.0/libusb.h>
#elif defined (HAVE_LIBUSB_H)
#include <libusb.h>
#else
#error Cannot find required libusb.h header
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "lirc_driver.h"

#include <ftdi.h>

#ifdef HAVE_LINUX_SCHED_H
#include <sched.h>
#include <linux/sched.h>
#endif

static const logchannel_t logchannel = LOG_DRIVER;

/* PID of the child process */
static pid_t child_pid = -1;

#define RXBUFSZ         2048
#define TXBUFSZ         65536

static char* device_config = NULL;
static int tx_baud_rate = 65536;
static int rx_baud_rate = 9600;
static int input_pin = 1;       /* RXD as input */
static int output_pin = 2;      /* RTS as output */
static int usb_vendor = 0x0403; /* default for FT232 */
static int usb_product = 0x6001;
static const char* usb_desc = NULL;
static const char* usb_serial = NULL;

static int laststate = -1;
static __u32 rxctr = 0;

static int pipe_main2tx[2] = { -1, -1 };
static int pipe_tx2main[2] = { -1, -1 };

#if 0
static lirc_t time_left(struct timeval* current, struct timeval* last, lirc_t gap)
{
	__u32 secs, diff;

	secs = current->tv_sec - last->tv_sec;

	diff = 1000000 * secs + current->tv_usec - last->tv_usec;

	return (lirc_t)(diff < gap ? gap - diff : 0);
}
#endif

static int modulate_pulses(unsigned char* buf, size_t size,
	const int* pulseptr, int n_pulses, __u32 f_sample, __u32 f_carrier,
	unsigned int duty_cycle);

static void list_devices(glob_t *buff)
{
	struct ftdi_context* ctx;
	struct ftdi_device_list* devlist;
	struct ftdi_device_list* dev;
	char vendor[128];
	char descr[128];
	int r;
	char device[256];

	ctx = ftdi_new();
	if (ctx == NULL) {
		log_error("List FTDI devices: ftdi_new() failed");
		return;
	}
	r = ftdi_usb_find_all(ctx, &devlist, 0, 0);
	if (r < 0) {
		log_error("List FTDI devices: _usb_find_all() failed");
		ftdi_free(ctx);
		return;
	}
	memset(buff, 0, sizeof(glob_t));
	buff->gl_offs = 32;
	buff->gl_pathv = calloc(buff->gl_offs, sizeof(char*));
	for (dev = devlist; dev != NULL; dev = dev->next) {
		r = ftdi_usb_get_strings(ctx,
					 dev->dev,
					 vendor, sizeof(vendor),
					 descr, sizeof(descr),
					 NULL, 0);
		if (r < 0) {
			log_warn("List FTDI devices: Cannot get strings");
			continue;
		}
		if (buff->gl_pathc >= buff->gl_offs) {
			log_warn("List FTDI devices - too many of them");
			break;
		}
		snprintf(device, sizeof(device),
			 "/dev/bus/usb/%03d/%03d:   %s:%s\n",
			 libusb_get_bus_number(dev->dev),
			 libusb_get_port_number(dev->dev),
			 vendor, descr);
		buff->gl_pathv[buff->gl_pathc] = strdup(device);
		buff->gl_pathc += 1;
	}
	ftdi_free(ctx);
}

static void parsesamples(unsigned char* buf, int n, int pipe_rxir_w)
{
	int i;
	lirc_t usecs;

	for (i = 0; i < n; i++) {
		int curstate = (buf[i] & (1 << input_pin)) != 0;

		rxctr++;
		if (curstate == laststate)
			continue;

		/* Convert number of samples to us.
		 *
		 * The datasheet indicates that the sample rate in
		 * bitbang mode is 16 times the baud rate but 32 seems
		 * to be correct. */
		usecs = (rxctr * 1000000LL) / (rx_baud_rate * 32);

		/* Clamp */
		if (usecs > PULSE_MASK)
			usecs = PULSE_MASK;

		/* Indicate pulse or bit */
		if (curstate)
			usecs |= PULSE_BIT;

		/* Send the sample */
		chk_write(pipe_rxir_w, &usecs, sizeof(usecs));

		/* Remember last state */
		laststate = curstate;
		rxctr = 0;
	}
}

static void child_process(int fd_rx2main, int fd_main2tx, int fd_tx2main)
{
	int ret = 0;
	struct ftdi_context ftdic;

	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	ftdi_init(&ftdic);

	/* indicate we're started: */
	ret = write(fd_tx2main, &ret, 1);

	while (1) {
		/* Open the USB device */
		if (ftdi_usb_open_desc(&ftdic, usb_vendor, usb_product, usb_desc, usb_serial) < 0) {
			log_error("unable to open FTDI device (%s)", ftdi_get_error_string(&ftdic));
			goto retry;
		}

		/* Enable bit-bang mode, setting output & input pins
		 * direction */
		if (ftdi_set_bitmode(&ftdic, 1 << output_pin, BITMODE_BITBANG) < 0) {
			log_error("unable to enable bitbang mode (%s)", ftdi_get_error_string(&ftdic));
			goto retry;
		}

		/* Set baud rate */
		if (ftdi_set_baudrate(&ftdic, rx_baud_rate) < 0) {
			log_error("unable to set required baud rate (%s)", ftdi_get_error_string(&ftdic));
			goto retry;
		}

		log_debug("opened FTDI device '%s' OK", drv.device);

		do {
			unsigned char buf[RXBUFSZ > TXBUFSZ ? RXBUFSZ : TXBUFSZ];

			/* transmit IR */
			ret = read(fd_main2tx, buf, sizeof(buf));
			if (ret > 0) {
				/* select correct transmit baudrate */
				if (ftdi_set_baudrate(&ftdic, tx_baud_rate) < 0) {
					log_error("unable to set required baud rate for transmission (%s)",
						  ftdi_get_error_string(&ftdic));
					goto retry;
				}
				if (ftdi_write_data(&ftdic, buf, ret) < 0)
					log_error("enable to write ftdi buffer (%s)",
						  ftdi_get_error_string(&ftdic));
				if (ftdi_usb_purge_tx_buffer(&ftdic) < 0)
					log_error("unable to purge ftdi buffer (%s)",
						  ftdi_get_error_string(&ftdic));

				/* back to rx baudrate: */
				if (ftdi_set_baudrate(&ftdic, rx_baud_rate) < 0) {
					log_error("unable to set restore baudrate for reception (%s)",
						  ftdi_get_error_string(&ftdic));
					goto retry;
				}

				/* signal transmission ready: */
				ret = write(fd_tx2main, &ret, 1);

				continue;
			} else if (ret == 0) {
				/* EOF => The parent has died so the pipe has closed */
				_exit(0);
			}

			/* receive IR */
			ret = ftdi_read_data(&ftdic, buf, RXBUFSZ);
			if (ret > 0)
				parsesamples(buf, ret, fd_rx2main);
		} while (ret > 0);

retry:
		/* Wait a while and try again */
		usleep(500000);
	}
}


static int drvctl_func(unsigned int cmd, void* arg)
{
	glob_t* glob;
	int i;

	switch (cmd) {
	case DRVCTL_GET_DEVICES:
		list_devices((glob_t*) arg);
		return 0;
	case DRVCTL_FREE_DEVICES:
		glob = (glob_t*) arg;
		for (i = 0; i < glob->gl_pathc; i += 1)
			free(glob->gl_pathv[i]);
		free(glob->gl_pathv);
		return 0;
	default:
		return DRV_ERR_NOT_IMPLEMENTED;
	}
}


static int hwftdi_init(void)
{
	int flags;
	int pipe_rx2main[2] = { -1, -1 };
	unsigned char buf[1];

	char* p;

	if (child_pid > 0) {
		log_info("hwftdi_init: Already initialised");
		return 1;
	}

	log_info("Initializing FTDI: %s", drv.device);

	/* Parse the device string, which has the form key=value,
	 * key=value, ...  This isn't very nice, but it's not a lot
	 * more complicated than what some of the other drivers do. */
	p = device_config = strdup(drv.device);
	while (p) {
		char* comma;
		char* value;

		comma = strchr(p, ',');
		if (comma != NULL)
			*comma = '\0';

		/* Skip empty options. */
		if (*p == '\0')
			goto next;

		value = strchr(p, '=');
		if (value == NULL) {
			log_error("device configuration option must contain an '=': '%s'", p);
			goto fail_start;
		}
		*value++ = '\0';

		if (strcmp(p, "vendor") == 0) {
			usb_vendor = strtol(value, NULL, 0);
		} else if (strcmp(p, "product") == 0) {
			usb_product = strtol(value, NULL, 0);
		} else if (strcmp(p, "desc") == 0) {
			usb_desc = value;
		} else if (strcmp(p, "serial") == 0) {
			usb_serial = value;
		} else if (strcmp(p, "input") == 0) {
			input_pin = strtol(value, NULL, 0);
		} else if (strcmp(p, "baud") == 0) {
			rx_baud_rate = strtol(value, NULL, 0);
		} else if (strcmp(p, "output") == 0) {
			output_pin = strtol(value, NULL, 0);
		} else if (strcmp(p, "txbaud") == 0) {
			tx_baud_rate = strtol(value, NULL, 0);
		} else {
			log_error("unrecognised device configuration option: '%s'", p);
			goto fail_start;
		}

next:
		if (comma == NULL)
			break;
		p = comma + 1;
	}

	rec_buffer_init();

	/* Allocate a pipe for lircd to read from */
	if (pipe(pipe_rx2main) == -1) {
		log_error("unable to create pipe_rx2main");
		goto fail_start;
	}
	if (pipe(pipe_main2tx) == -1) {
		log_error("unable to create pipe_main2tx");
		goto fail_main2tx;
	}
	if (pipe(pipe_tx2main) == -1) {
		log_error("unable to create pipe_tx2main");
		goto fail_tx2main;
	}

	drv.fd = pipe_rx2main[0];

	flags = fcntl(drv.fd, F_GETFL);

	/* make the read end of the pipe non-blocking: */
	if (fcntl(drv.fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		log_error("unable to make pipe read end non-blocking");
		goto fail;
	}

	/* Make the read end of the send pipe non-blocking */
	flags = fcntl(pipe_main2tx[0], F_GETFL);
	if (fcntl(pipe_main2tx[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		log_error("unable to make pipe read end non-blocking");
		goto fail;
	}

	/* Spawn the child process */
	child_pid = fork();
	if (child_pid == -1) {
		log_error("unable to fork child process");
		goto fail;
	} else if (child_pid == 0) {
		/* we're the child: */
		close(pipe_rx2main[0]);
		close(pipe_main2tx[1]);
		close(pipe_tx2main[0]);
		child_process(pipe_rx2main[1], pipe_main2tx[0], pipe_tx2main[1]);
	}

	/* we're the parent: */
	close(pipe_rx2main[1]);
	close(pipe_main2tx[0]);
	pipe_main2tx[0] = -1;
	close(pipe_tx2main[1]);
	pipe_tx2main[1] = -1;

	/* wait for child to be started */
	chk_read(pipe_tx2main[0], &buf, 1);

	return 1;

fail:
	drv.fd = -1;

	close(pipe_tx2main[0]);
	close(pipe_tx2main[1]);
	pipe_tx2main[0] = -1;
	pipe_tx2main[1] = -1;

fail_tx2main:
	close(pipe_main2tx[0]);
	close(pipe_main2tx[1]);
	pipe_main2tx[0] = -1;
	pipe_main2tx[1] = -1;

fail_main2tx:
	close(pipe_rx2main[0]);
	close(pipe_rx2main[1]);

fail_start:
	if (device_config != NULL) {
		free(device_config);
		device_config = NULL;
	}

	return 0;
}

static int hwftdi_close(void)
{
	if (child_pid != -1) {
		/* Kill the child process, and wait for it to exit */
		if (kill(child_pid, SIGTERM) == -1)
			return 0;
		if (waitpid(child_pid, NULL, 0) == 0)
			return 0;
		child_pid = -1;
	}

	close(drv.fd);
	drv.fd = -1;

	close(pipe_main2tx[1]);
	pipe_main2tx[1] = -1;
	close(pipe_tx2main[0]);
	pipe_tx2main[0] = -1;

	free(device_config);
	device_config = NULL;

	return 1;
}

static char* hwftdi_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

static lirc_t hwftdi_readdata(lirc_t timeout)
{
	int n;
	lirc_t res = 0;

	if (!waitfordata((long)timeout))
		return 0;

	n = read(drv.fd, &res, sizeof(res));
	if (n != sizeof(res))
		res = 0;

	return res;
}

static ssize_t write_pulse(unsigned char* buf, size_t size,
	struct ir_remote* remote, struct ir_ncode* code)
{
	__u32 f_sample = tx_baud_rate * 8;
	__u32 f_carrier = remote->freq == 0 ? DEFAULT_FREQ : remote->freq;
	const lirc_t* pulseptr;
	int n_pulses;

	log_debug("hwftdi_send() carrier=%dHz f_sample=%dHz ", f_carrier, f_sample);

	/* initialize decoded buffer: */
	if (!send_buffer_put(remote, code))
		return -1;

	/* init vars: */
	n_pulses = send_buffer_length();
	pulseptr = send_buffer_data();

	return modulate_pulses(buf, size, pulseptr, n_pulses, f_sample, f_carrier,
	    remote->duty_cycle);
}

static int modulate_pulses(unsigned char* buf, size_t size,
	const int* pulseptr, int n_pulses, __u32 f_sample, __u32 f_carrier,
	unsigned int duty_cycle)
{
	__u32 div_carrier;
	__u32 duty;
	lirc_t pulse;
	int pulsewidth;
	int val_carrier;
	int bufidx;
	int sendpulse;

	bufidx = 0;
	div_carrier = 0;
	val_carrier = 0;
	sendpulse = 0;

	/* Calculate the carrier on time (in # samples) */
	duty = f_sample * duty_cycle / 100;  /* duty_cycle is 1-100 */
	if (duty <= 1)
		duty = 1;
	else if (duty >= f_sample)
		duty = f_sample - 1;

	while (n_pulses--) {
		/* take pulse from buffer */
		pulse = *pulseptr++;

		/* compute the pulsewidth (in # samples) */
		pulsewidth = ((__u64)f_sample) * ((__u32)(pulse & PULSE_MASK)) / 1000000ul;

		/* toggle pulse / space */
		sendpulse = sendpulse ? 0 : 1;

		while (pulsewidth--) {
			/* carrier generator (generates a carrier
			 * continously, will be modulated by the
			 * requested signal): */
			div_carrier += f_carrier;
			if (div_carrier >= f_sample) {
				div_carrier -= f_sample;
			}
			val_carrier = div_carrier < duty ? 255 : 0;

			/* send carrier or send space ? */
			if (sendpulse)
				buf[bufidx++] = val_carrier;
			else
				buf[bufidx++] = 0;

			/* flush txbuffer? */
			/* note: be sure to have room for last '0' */
			if (bufidx >= (size - 1)) {
				log_error("buffer overflow while generating IR pattern");
				return -1;
			}
		}
	}

	/* always end with 0 to turn off transmitter: */
	buf[bufidx++] = 0;
	return bufidx;
}

static int hwftdi_send(struct ir_remote* remote, struct ir_ncode* code)
{
	unsigned char buf[TXBUFSZ];
	ssize_t pulse_len;

	pulse_len = write_pulse(buf, sizeof(buf), remote, code);
	if (pulse_len == -1)
		return 0;

	/* let the child process transmit the pattern */
	chk_write(pipe_main2tx[1], buf, pulse_len);

	/* wait for child process to be ready with it */
	chk_read(pipe_tx2main[0], buf, 1);

	return 1;
}

/*
* Mode2 transmitter using the bitbang mode of an FTDI USB-to-serial chip such as
* the FT230X.  The standard FTDI driver with the FT232R is unreliable having a
* IR failure rate of ~2%.  The FT232R also becomes unusable if you set the
* tx_baud_rate to values other than 65536.
*
* This driver works differently modifying its tx_baud_rate according to the
* carrier frequency set.  This is ok because the FT230X does not have the timing
* bugs present in the FT232R.  This allows much lower data rates over USB and
* so improves reliablity, particularly if other devices are connect to the same
* USB port.
*
* Receive is not included to significantly simplify design of this driver.
*/

struct ftdix_config {
	uint32_t vendor;
	uint32_t product;
	char* desc;
	char* serial;
	uint32_t output;

	char* _config_text;
};

static struct ftdix_config ftdix_default_config = {
	.vendor = 0x0403,
	.product = 0x6015,
	.output = 2,  /* RTS as output */
};

static int is_open = 0;
static struct ftdi_context ftdic;

static int parse_config(const char* device_config, struct ftdix_config* config);
static void hwftdix_clear_config(struct ftdix_config* config);

static int hwftdix_open(const char* device)
{
	struct ftdix_config config = {0};

	if (is_open) {
		log_info("Ignoring attempt to reopen ftdi device");
		return 0;
	}

	log_info("Opening FTDI-X device: %s", device);

	if (parse_config(device, &config) != 0) {
		goto fail;
	}

	drv.fd = -1;

	if (ftdi_init(&ftdic) < 0) {
		log_error(
			"ftdi_init failed: %s", ftdi_get_error_string(&ftdic));
		goto fail;
	}

	/* Open the USB device */
	if (ftdi_usb_open_desc(&ftdic, config.vendor, config.product,
			       config.desc, config.serial) < 0) {
		log_error("unable to open FTDI device (%s)",
			  ftdi_get_error_string(&ftdic));
		goto fail_inited;
	}

	/* Enable bit-bang mode, setting output & input pins direction */
	if (ftdi_set_bitmode(&ftdic, 1 << config.output, BITMODE_BITBANG) < 0) {
		log_error("unable to enable bitbang mode (%s)",
			  ftdi_get_error_string(&ftdic));
		goto fail_opened;
	}

	log_debug("opened FTDI device '%s' OK", device);
	is_open = 1;
	return 0;
fail_opened:
	ftdi_usb_close(&ftdic);
fail_inited:
	ftdi_deinit(&ftdic);
	hwftdix_clear_config(&config);
fail:
	log_debug("Failed to open FTDI device '%s'", device);
	return 1;
}

static int hwftdix_close(void)
{
	if (ftdi_usb_close(&ftdic) < 0) {
		log_error("ftdi_usb_close() failed: %s",
			  ftdi_get_error_string(&ftdic));
	}
	ftdi_deinit(&ftdic);
	is_open = 0;
	return 0;
}

static void sched_enable_realtime(int* orig_scheduler)
{
#ifdef HAVE_LINUX_SCHED_H
	*orig_scheduler = sched_getscheduler(0);
	if (*orig_scheduler == -1) {
		log_warn("Failed to get current scheduling policy with error "
			 "%s  Sending will not run with real-time priority and "
			 "you may suffer USB buffer underruns causing corrupt "
			 "IR signals", strerror(errno));
		return;
	}
	if (*orig_scheduler != SCHED_BATCH && *orig_scheduler != SCHED_OTHER &&
	    *orig_scheduler != SCHED_IDLE) {
		/* We only know how to restore these schedulers */
		*orig_scheduler = -1;
		return;
	}
	struct sched_param param = {
		.sched_priority = 1,
	};
	if (sched_setscheduler(0, SCHED_FIFO, &param) < 0) {
		log_warn("Failed to set scheduling policy to SCHED_FIFO: %s "
			 "Sending will not run with real-time priority and you "
			 "may suffer USB buffer underruns causing corrupt IR "
			 "signals", strerror(errno));
		*orig_scheduler = -1;
	}
#else
	*orig_scheduler = -1;
#endif /* HAVE_LINUX_SCHED_H */
}

static void sched_restore(int* orig_scheduler)
{
#ifdef HAVE_LINUX_SCHED_H
	if (*orig_scheduler == -1)
		return;

	struct sched_param param = {
		.sched_priority = 0,
	};
	if (sched_setscheduler(0, *orig_scheduler, &param) < 0) {
		log_warn("Restoring scheduling policy failed: %s",
			 strerror(errno));
	}
#else
	*orig_scheduler = -1;
#endif /* HAVE_LINUX_SCHED_H */
}


static int parse_config(const char* device_config, struct ftdix_config* config)
{
	char* p;

	*config = ftdix_default_config;

	/* Parse the device string, which has the form key=value,
	 * key=value, ...  This isn't very nice, but it's not a lot
	 * more complicated than what some of the other drivers do. */
	config->_config_text = p = strdup(device_config);
	assert(p);
	while (p) {
		char* comma;
		char* value;

		comma = strchr(p, ',');
		if (comma != NULL)
			*comma = '\0';

		/* Skip empty options. */
		if (*p == '\0')
			goto next;

		value = strchr(p, '=');
		if (value == NULL) {
			log_error("device configuration option must contain an "
				  "'=': '%s'", p);
			goto error;
		}
		*value++ = '\0';

		if (strcmp(p, "vendor") == 0) {
			config->vendor = strtol(value, NULL, 0);
		} else if (strcmp(p, "product") == 0) {
			config->product = strtol(value, NULL, 0);
		} else if (strcmp(p, "desc") == 0) {
			config->desc = value;
		} else if (strcmp(p, "serial") == 0) {
			config->serial = value;
		} else if (strcmp(p, "output") == 0) {
			config->output = strtol(value, NULL, 0);
		} else {
			log_error("unrecognised device configuration option: "
				  "'%s'", p);
			goto error;
		}

next:
		if (comma == NULL)
			break;
		p = comma + 1;
	}
	return 0;
error:
	hwftdix_clear_config(config);
	return 1;
}

static void hwftdix_clear_config(struct ftdix_config* config)
{
	free(config->_config_text);
	memset(config, 0, sizeof(*config));
}


static int hwftdix_send(struct ir_remote* remote, struct ir_ncode* code)
{
	int success = 1;
	unsigned char buf[TXBUFSZ];
	ssize_t buf_len;
	int orig_scheduler;
	__u32 f_carrier = remote->freq == 0 ? DEFAULT_FREQ : remote->freq;

	/* A sample rate of carrier*2 means we will get the pattern 1010101010
	 * when the blaster is on and 0000000000 when it is off. */
	__u32 f_sample = f_carrier * 2;
	__u32 tx_baud = f_carrier * 2 / 64;

	const lirc_t* pulseptr;
	int n_pulses;

	log_debug("hwftdix_send() carrier=%dHz f_sample=%dHz tx_baud=%d",
		  f_carrier, f_sample, tx_baud);

	/* initialize decoded buffer: */
	if (!send_buffer_put(remote, code))
		return -1;

	/* init vars: */
	n_pulses = send_buffer_length();
	pulseptr = send_buffer_data();

	buf_len = modulate_pulses(buf, sizeof(buf), pulseptr, n_pulses, f_sample,
				  f_carrier, remote->duty_cycle);

	/* select correct transmit baudrate */
	if (ftdi_set_baudrate(&ftdic, tx_baud) < 0) {
		log_error("unable to set required baud rate for transmission "
			  "(%s)", ftdi_get_error_string(&ftdic));
		success = 0;
		goto out;
	}

	/* buffer underruns will mess up our IR signal, go into realtime mode
	   if possible: */
	sched_enable_realtime(&orig_scheduler);

	if (ftdi_write_data(&ftdic, buf, buf_len) < buf_len) {
		log_error("enable to write ftdi buffer (%s)",
			  ftdi_get_error_string(&ftdic));
		return 1;
	}

	sched_restore(&orig_scheduler);
out:
	return success;
}

const struct driver hw_ftdi = {
	.name		= "ftdi-exp",
	.device		= "",
	.features	= LIRC_CAN_REC_MODE2 | \
			  LIRC_CAN_SEND_PULSE | \
			  LIRC_CAN_SET_SEND_CARRIER,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= hwftdi_init,
	.open_func	= default_open,
	.close_func	= hwftdi_close,
	.send_func	= hwftdi_send,
	.rec_func	= hwftdi_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= drvctl_func,
	.readdata	= hwftdi_readdata,
	.api_version	= 3,
	.driver_version = "0.9.4",
	.info		= "Refactored, experimental update of ftdi",
	.device_hint    = "drvctl",
};


const struct driver hw_ftdix = {
	.name		= "ftdix",
	.device		= "",
	.features	= LIRC_CAN_SEND_PULSE | \
			  LIRC_CAN_SET_SEND_CARRIER,
	.send_mode	= LIRC_MODE_PULSE,
	.rec_mode	= 0,
	.code_length	= 0,
	.init_func	= NULL,
	.open_func	= hwftdix_open,
	.close_func	= hwftdix_close,
	.send_func	= hwftdix_send,
	.rec_func	= NULL,
	.decode_func	= receive_decode,
	.drvctl_func	= drvctl_func,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "See file://" PLUGINDOCS "/ftdix.html",
	.device_hint    = "drvctl",
};

const struct driver* hardwares[] = {
	&hw_ftdi, &hw_ftdix, (const struct driver*)NULL };

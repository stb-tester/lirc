
/****************************************************************************
 ** hw_defaulmsendt.c ************************************************************
 ****************************************************************************
 *
 * driver for simsend internal tests.
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "lirc_driver.h"

#include "default.h"

extern struct ir_remote *repeat_remote;

static __u32 supported_send_modes[] = {
	/* LIRC_CAN_SEND_LIRCCODE, */
	/* LIRC_CAN_SEND_MODE2, this one would be very easy */
	LIRC_CAN_SEND_PULSE,
	/* LIRC_CAN_SEND_RAW, */
	0
};

static __u32 supported_rec_modes[] = {
	LIRC_CAN_REC_LIRCCODE,
	LIRC_CAN_REC_MODE2,
	/* LIRC_CAN_REC_PULSE, shouldn't be too hard */
	/* LIRC_CAN_REC_RAW, */
	0
};

static const const struct hardware hw_simsend = {
	LIRC_DRIVER_DEVICE,	/* default device */
	-1,			/* fd */
	0,			/* features */
	0,			/* send_mode */
	0,			/* rec_mode */
	0,			/* code_length */
	default_init,		/* init_func */
	default_deinit,		/* deinit_func */
	default_send,		/* send_func */
	default_rec,		/* rec_func */
	receive_decode,		/* decode_func */
	default_ioctl,		/* ioctl_func */
	default_readdata,
	"simsend"
};

const struct hardware* hardwares[] = { &hw_simsend, (const struct hardware*)NULL };


/**********************************************************************
 *
 * internal function prototypes
 *
 **********************************************************************/

static int write_send_buffer(int lirc);

/**********************************************************************
 *
 * decode stuff
 *
 **********************************************************************/

int default_readdata(lirc_t timeout)
{
	int data, ret;

	if (!waitfordata((long)timeout))
		return 0;

#if defined(SIM_REC) && !defined(DAEMONIZE)
	while (1) {
		__u32 scan;

		ret = fscanf(stdin, "space %u\n", &scan);
		if (ret == 1) {
			data = (int) scan;
			break;
		}

		ret = fscanf(stdin, "pulse %u\n", &scan);
		if (ret == 1) {
			data = (int) scan | PULSE_BIT;
			break;
		}

		ret = fscanf(stdin, "%*s\n");
		if (ret == EOF)
			dosigterm(SIGTERM);
	}
#else
	ret = read(hw.fd, &data, sizeof(data));
	if (ret != sizeof(data)) {
		logprintf(LOG_ERR, "error reading from %s (ret %d, expected %d)",
			  hw.device, ret, sizeof(data));
		logperror(LOG_ERR, NULL);
		default_deinit();

		return 0;
	}

	if (data == 0) {
		static int data_warning = 1;

		if (data_warning) {
			logprintf(LOG_WARNING, "read invalid data from device %s", hw.device);
			data_warning = 0;
		}
		data = 1;
	}
#endif
	return data ;
}

/*
  interface functions
*/

int default_init()
{
	hw.fd = STDOUT_FILENO;
	hw.features = LIRC_CAN_SEND_PULSE;
	hw.send_mode = LIRC_MODE_PULSE;
	hw.rec_mode = 0;
	return (1);
}

int default_deinit(void)
{
	return (1);
}

static int write_send_buffer(int lirc)
{
	int i;

	if (send_buffer.wptr == 0) {
		LOGPRINTF(1, "nothing to send");
		return (0);
	}
	for (i = 0;;) {
		printf("pulse %u\n", (__u32) send_buffer.data[i++]);
		if (i >= send_buffer.wptr)
			break;
		printf("space %u\n", (__u32) send_buffer.data[i++]);
	}
	return (send_buffer.wptr * sizeof(lirc_t));
}

int default_send(struct ir_remote *remote, struct ir_ncode *code)
{
	/* things are easy, because we only support one mode */
	if (hw.send_mode != LIRC_MODE_PULSE)
		return (0);

	if (!init_send(remote, code))
		return (0);

	if (write_send_buffer(hw.fd) == -1) {
		logprintf(LOG_ERR, "write failed");
		logperror(LOG_ERR, NULL);
		return (0);
	} else {
		printf("space %u\n", (__u32) remote->min_remaining_gap);
	}
	return (1);
}

char *default_rec(struct ir_remote *remotes)
{
	if (!clear_rec_buffer()) {
		default_deinit();
		return NULL;
	}
	return (decode_all(remotes));
}

int default_ioctl(unsigned int cmd, void *arg)
{
	return ioctl(hw.fd, cmd, arg);
}

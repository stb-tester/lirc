
/****************************************************************************
 ** hw_receive.c ************************************************************
 ****************************************************************************
 *
 * routines for regression test receiving
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


//Forwards:
int default_init(void);
int default_config(struct ir_remote *remotes);
int default_deinit(void);
int default_send(struct ir_remote *remote, struct ir_ncode *code);
char *default_rec(struct ir_remote *remotes);
int default_ioctl(unsigned int cmd, void *arg);
lirc_t default_readdata(lirc_t timeout);

static const const struct driver hw_simreceive = {
	.name		=	"simreceive",
	.device		=	LIRC_DRIVER_DEVICE,
	.features	=	0,
	.send_mode	=	0,
	.rec_mode	=	0,
	.code_length	=	0,
	.init_func	=	default_init,
	.deinit_func	=	default_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	default_send,
	.rec_func	=	default_rec,
	.decode_func	=	receive_decode,
	.drvctl_func	=	default_ioctl,
	.readdata	=	default_readdata,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=	"No info available"
};


const struct driver* hardwares[] = { &hw_simreceive, (const struct driver*)NULL };


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
		if (ret == EOF){
			//logprintf(LIRC_NOTICE, "simreceive: EOF");
			//dosigterm(SIGTERM);
			kill(getpid(), SIGUSR1);
		}
	}
	return data ;
}

/*
  interface functions
*/

int default_init()
{
	drv.fd = STDIN_FILENO;
	drv.features = LIRC_CAN_REC_MODE2;
	drv.send_mode = 0;
	drv.rec_mode = LIRC_MODE_MODE2;
	return (1);
}

int default_deinit(void)
{
	return (1);
}

static int write_send_buffer(int lirc)
{
	if (send_buffer_length() == 0) {
		LOGPRINTF(1, "nothing to send");
		return (0);
	}
	return (write(lirc, send_buffer_data(), send_buffer_length() * sizeof(lirc_t)));
}

int default_send(struct ir_remote *remote, struct ir_ncode *code)
{
	/* things are easy, because we only support one mode */
	if (drv.send_mode != LIRC_MODE_PULSE)
		return (0);

	if (!send_buffer_put(remote, code))
		return (0);

	if (write_send_buffer(drv.fd) == -1) {
		logprintf(LIRC_ERROR, "write failed");
		logperror(LIRC_ERROR, NULL);
		return (0);
	}
	return (1);
}

char *default_rec(struct ir_remote *remotes)
{
	if (!rec_buffer_clear()) {
		default_deinit();
		return NULL;
	}
	return (decode_all(remotes));
}

int default_ioctl(unsigned int cmd, void *arg)
{
	return ioctl(drv.fd, cmd, arg);
}

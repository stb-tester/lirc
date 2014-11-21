/****************************************************************************
 ** udp.c ****************************************************************
 ****************************************************************************
 *
 * receive mode2 input via UDP
 *
 * Copyright (C) 2002 Jim Paris <jim@jtan.com>
 *
 * Distribute under GPL version 2 or later.
 *
 * Received UDP packets consist of some number of LE 16-bit integers.
 * The high bit signifies whether the received signal was high or low;
 * the low 15 bits specify the number of 1/16384-second intervals the
 * signal lasted.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <errno.h>

#include "lirc_driver.h"

static int zerofd;		/* /dev/zero */
static int sockfd;		/* the socket */

int udp_init()
{
	int port;
	struct sockaddr_in addr;

	logprintf(LIRC_INFO, "Initializing UDP: %s", drv.device);

	rec_buffer_init();

	port = atoi(drv.device);
	if (port == 0) {
		logprintf(LIRC_ERROR, "invalid port: %s", drv.device);
		return 0;
	}

	/* drv.fd needs to point somewhere when we have extra data */
	if ((zerofd = open("/dev/zero", O_RDONLY)) < 0) {
		logprintf(LIRC_ERROR, "can't open /dev/zero: %s", strerror(errno));
		return 0;
	}

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		logprintf(LIRC_ERROR, "error creating socket: %s", strerror(errno));
		close(zerofd);
		return 0;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		logprintf(LIRC_ERROR, "can't bind socket to port %d: %s", port, strerror(errno));
		close(sockfd);
		close(zerofd);
		return 0;
	}

	logprintf(LIRC_INFO, "Listening on port %d/udp", port);

	drv.fd = sockfd;

	return (1);
}

int udp_deinit(void)
{
	close(sockfd);
	close(zerofd);
	drv.fd = -1;
	return (1);
}

char *udp_rec(struct ir_remote *remotes)
{
	if (!rec_buffer_clear())
		return (NULL);
	return (decode_all(remotes));
}

lirc_t udp_readdata(lirc_t timeout)
{
	static u_int8_t buffer[8192];
	static int buflen = 0;
	static int bufptr = 0;
	lirc_t data;
	u_int8_t packed[2];
	u_int32_t tmp;

	/* Assume buffer is empty; LIRC should select on the socket */
	drv.fd = sockfd;

	/* If buffer is empty, get data into it */
	if ((bufptr + 2) > buflen) {
		if (!waitfordata(timeout))
			return 0;
		if ((buflen = recv(sockfd, &buffer, sizeof(buffer), 0)) < 0) {
			logprintf(LIRC_INFO, "Error reading from UDP socket");
			return 0;
		}
		if (buflen & 1)
			buflen--;
		if (buflen == 0)
			return 0;
		bufptr = 0;
	}

	/* Read as 2 bytes to avoid endian-ness issues */
	packed[0] = buffer[bufptr++];
	packed[1] = buffer[bufptr++];

	/* TODO: This assumes the receiver is active low.  Should
	   be specified by user, or autodetected.  */
	data = (packed[1] & 0x80) ? 0 : PULSE_BIT;

	/* Convert 1/16384-seconds to microseconds */
	tmp = (((u_int32_t) packed[1]) << 8) | packed[0];
	/* tmp = ((tmp & 0x7FFF) * 1000000) / 16384; */
	/* prevent integer overflow: */
	tmp = ((tmp & 0x7FFF) * 15625) / 256;

	data |= tmp & PULSE_MASK;

	/* If our buffer still has data, give LIRC /dev/zero to select on */
	if ((bufptr + 2) <= buflen)
		drv.fd = zerofd;

	return (data);
}

const struct driver hw_udp = {
	.name		=	"udp",
	.device		=	"8765",
	.features	=	LIRC_CAN_REC_MODE2,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_MODE2,
	.code_length	=	0,
	.init_func	=	udp_init,
	.deinit_func	=	udp_deinit,
        .open_func      =       default_open,
        .close_func     =       default_close,
	.send_func	=	NULL,
	.rec_func	=	udp_rec,
	.decode_func	=	receive_decode,
	.drvctl_func	=	NULL,
	.readdata	=	udp_readdata,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=	"No info available"
};

const struct driver* hardwares[] = { &hw_udp, (const struct driver*)NULL };

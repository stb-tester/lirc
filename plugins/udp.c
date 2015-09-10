/****************************************************************************
 ** udp.c *******************************************************************
 ****************************************************************************
 *
 * Copyright (C) 2002 Jim Paris <jim@jtan.com>
 *
 * Distribute under GPL version 2 or later.
 *
 */

/**
 * \file udp.c
 *
 * \brief Receive mode2 input via UDP
 *
 * Received UDP packets consist of a number of little endian 16-bit integers.
 * The high bit signifies the state of the received signal;
 * set indicates a mark, clear a space.
 * The low 15 bits specify how long the signal lasted.
 *
 * With the default resolution setting (61) the times are measured in 1/16384
 * second intervals. This was used by old hardware using a cheap 32kHz clock
 * crystal, when designing new devices a 1MHz 1µs clock is recommended.
 *
 * To allow long times to be transfered a long UDP input format is available,
 * this consists of a zero time as specified by the short format followed by
 * a four byte little endian time value. This should only be needed occasionally
 * for example the time between button presses.
 *
 * The UDP port can be set using the `--device=port` or `-d port` command line
 * switch.
 *
 * The timing resolution can be set using the `--driver-option=clocktick:value`
 * or `-A clocktick:value` command line switch.
 *
 * e.g.
 *
 *     mode2 --driver=udp --device=8766 --driver-option=clocktick:1
 *
 * to use port 8766 and 1 microsecond timing resolution.
 *
 * \note Little endian is not conventional network byte order.
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

static const logchannel_t logchannel = LOG_DRIVER;

static int zerofd;              /* /dev/zero */
static int sockfd;              /* the socket */


/**
 * Driver control.
 *
 * Process the --driver-option key:value pairs.
 *
 *  clocktick:value
 *	Set the timing resolution to specified value.
 *
 * \retval 0	Success.
 * \retval !=0	drvctl error.
 */
static int udp_drvctl_func(unsigned int cmd, void* arg)
{
	struct option_t* opt;
	long value;

	switch (cmd) {
	case DRVCTL_SET_OPTION:
		opt = (struct option_t*)arg;
		if (strcmp(opt->key, "clocktick") == 0) {
			value = strtol(opt->value, NULL, 10);
			/* IR mark & space times are typically < 1ms */
			if (value < 1 || 1000 < value) {
				log_error("invalid clock period: %s", drv.device);
				return DRV_ERR_BAD_VALUE;
			}
			drv.resolution = value;
			return 0;
		} else {
			return DRV_ERR_BAD_OPTION;
		}
		break;

	default:
		return DRV_ERR_NOT_IMPLEMENTED;
	}
	return 0;
}

/**
 * Initialize the driver.
 *
 * Open the UDP port as specified by the device command line switch
 * or use the default values: port = 8765, resolution = 61.
 *
 * \retval 1       Success.
 * \retval 0       Error.
 */
int udp_init(void)
{
	int count;
	unsigned int port;
	struct sockaddr_in addr;

	log_info("Initializing UDP: %s", drv.device);

	rec_buffer_init();

	/* device string is "port" */
	count = sscanf(drv.device, "%u", &port);
	if (count != 1 || port < 1 || 65535 < port) {
		log_error("invalid port: %s", drv.device);
	return 0;
	}

	log_notice("using UDP port: %d, resolution: %d", port, drv.resolution);

	/* drv.fd needs to point somewhere when we have extra data */
	zerofd = open("/dev/zero", O_RDONLY);
	if (zerofd < 0) {
		log_error("can't open /dev/zero: %s", strerror(errno));
		return 0;
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		log_error("error creating socket: %s", strerror(errno));
		close(zerofd);
		return 0;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		log_error("can't bind socket to port %d: %s", port, strerror(errno));
		close(sockfd);
		close(zerofd);
		return 0;
	}

	log_info("Listening on port %d/udp", port);

	drv.fd = sockfd;

	return 1;
}

/**
 * Deinitialize the driver (close the ports).
 * \retval 1       Success.
 * \retval 0       Error.
 */
int udp_deinit(void)
{
	close(sockfd);
	close(zerofd);
	drv.fd = -1;
	return 1;
}

/**
 * Receive data from remote.
 *
 * The UDP driver just calls the standard functions.
 */
char* udp_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

/**
 * Read data from the UDP port.
 *
 * Data read from the UDP port is converted to lirc mode2 format and returned
 * one measured time interval at a time.
 * \param timeout  Time to wait for data.
 * \return         IR timing data in lirc mode2 format.
 */
lirc_t udp_readdata(lirc_t timeout)
{
	static u_int8_t buffer[8192];
	static int buflen = 0;
	static int bufptr = 0;
	lirc_t data;
	u_int8_t packed[4];
	u_int64_t tmp;

	/* Assume buffer is empty; LIRC should select on the socket */
	drv.fd = sockfd;

	/* If buffer is empty, get data into it */
	if ((bufptr + 2) > buflen) {
		if (!waitfordata(timeout))
			return 0;
		buflen = recv(sockfd, &buffer, sizeof(buffer), 0);
		if (buflen < 0) {
			log_info("Error reading from UDP socket");
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

	/* Low indicates that the receiver has detected the marking state
	 * i.e. is receiving IR pulses.
	 */
	data = (packed[1] & 0x80) ? 0 : PULSE_BIT;

	tmp = (((u_int32_t) packed[1]) << 8) | packed[0];
	tmp &= 0x7FFF;
	if (tmp == 0) {
		/*
		 * A zero flags the following  bytes as an extended time value.
		 * If buffer is empty, get data into it.
		 */
		if ((bufptr + 4) > buflen) {
			if (!waitfordata(timeout))
				return 0;
			buflen = recv(sockfd, &buffer, sizeof(buffer), 0);
			if (buflen < 0) {
				log_info("Error reading from UDP socket");
				return 0;
			}
			if (buflen & 1)
				buflen--;
			if (buflen == 0)
				return 0;
			bufptr = 0;
		}
		/* Read as 4 bytes to avoid endian-ness issues. */
		packed[0] = buffer[bufptr++];
		packed[1] = buffer[bufptr++];
		packed[2] = buffer[bufptr++];
		packed[3] = buffer[bufptr++];
		tmp = (((u_int64_t) packed[3]) << 24)
		    | (((u_int64_t) packed[2]) << 16)
		    | (((u_int64_t) packed[1]) << 8)
		    | packed[0];
	}
	switch (drv.resolution) {
	case 1:
		break;
	/*
	 * TODO: This case handles the way the code used to be,
	 * 1/16384-second or 61.03515625 microseconds are only
	 * a fraction of a percent from 61us so do not really
	 * justify special treatment.
	 *
	case 61:
		* Convert 1/16384-seconds to microseconds *
		* tmp = (tmp * 1000000) / 16384; *
		* prevent integer overflow: *
		tmp = (tmp * 15625) / 256;
		break;
	*/
	default:
		tmp *= drv.resolution;
	}
	if (tmp > PULSE_MASK)
		tmp = PULSE_MASK;

	data |= tmp;

	/* If our buffer still has data, give LIRC /dev/zero to select on */
	if ((bufptr + 2) <= buflen)
		drv.fd = zerofd;

	return data;
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
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	NULL,
	.rec_func	=	udp_rec,
	.decode_func	=	receive_decode,
	.drvctl_func	=	udp_drvctl_func,
	.readdata	=	udp_readdata,
	.resolution	=	61,
	.api_version	=	3,
	.driver_version =	"0.9.3",
	.info		=	"See file://" PLUGINDOCS "/udp.html",
	.device_hint    = "udp_port",
};

const struct driver* hardwares[] = { &hw_udp, (const struct driver*)NULL };

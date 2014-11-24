/***************************************************************************
 *   Copyright (C) 2009 by M. Feser                                        *
 *   Modified to work with Y.A.R.D.2 USB                                   *
 *   (c) Copyright 2012 R. Geigenberger / Frank Kohlmann                   *
 *                                                                         *
 *   This driver is for YARD2 USB IR Receiver                              * 
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "lirc_driver.h"

#ifndef MSG_NOSIGNAL
// MSG_NOSIGNAL is not defined in MacOSX. (Is this even needed
// since    lircd.c sets SIGPIPE to SIG_IGN?)
#define MSG_NOSIGNAL 0
#endif

/* Defines */
#define YARDSRV_SOCK_PATH	"/tmp/yardsrv_sock"
#define SRVCMD_IRREG		0xEE
#define SRVCMD_IRUNREG		0xDD
#define SRVRES_SUCCESS		0x00
#define SRVRES_ERROR		0xFF
#define IRCODE_NUM_BYTES	6
#define IRCODE_NUM_BITS		( IRCODE_NUM_BYTES * 8 )

#define SYS_IR_REC	 0x80
#define SYS_IR_SEND  0x81

#define YARD2_URL   "https://www.assembla.com/code/yard2srvd/git/nodes"

typedef unsigned char YARD_IRCODE[6];

/* Variables */
static struct timeval start, end, last;
static ir_code code;		//64bit int

/* Export function */
static int yard2_decode(struct ir_remote *remote, struct decode_ctx_t *ctx);
static int yard2_init(void);
static int yard2_deinit(void);
static char* yard2_rec(struct ir_remote *remotes);
static int yard2_send(struct ir_remote *remote, struct ir_ncode *icode);

const struct driver hw_yard2 = {
	.name 		= "yard2",
	.device 	= "",
	.features 	= LIRC_CAN_REC_LIRCCODE | LIRC_CAN_SEND_LIRCCODE,
	.send_mode 	= LIRC_MODE_PULSE,
	.rec_mode 	= LIRC_MODE_LIRCCODE,
	.code_length 	= IRCODE_NUM_BYTES,
	.init_func 	= yard2_init,
	.deinit_func 	= yard2_deinit,
	.send_func 	= yard2_send,
	.rec_func 	= yard2_rec,
	.decode_func 	= yard2_decode,
	.drvctl_func 	= default_drvctl,
	.readdata 	= NULL,
	.api_version 	= 2,
	.driver_version	= "0.9.2",
	.open_func 	= default_open,
	.close_func 	= default_close,
	.driver_version	= "0.9.2",
	.info 		= "Driver for the yard2 DIY assembly kit." \
			  "See: " YARD2_URL
};
const struct driver* hardwares[] = { &hw_yard2, (const struct driver *)NULL };

/* Implementation */

static int yard2_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 0, 0, IRCODE_NUM_BITS, code, 0, 0)) {
		return 0;
	}
	map_gap(remote, ctx, &start, &last, 0);	// FIXME: Check 0 vs signal_length
	return 1;
}

static int yard2_init(void)
{
	struct sockaddr_un srvAddr;
	int srvAddrLen;

	// Establish connection to YARD server
	bzero((char *)&srvAddr, sizeof(srvAddr));
	srvAddr.sun_family = AF_UNIX;
	strcpy(srvAddr.sun_path, YARDSRV_SOCK_PATH);
	srvAddrLen = strlen(srvAddr.sun_path) + sizeof(srvAddr.sun_family);

	drv.fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (drv.fd < 0) {
		logprintf(LIRC_ERROR, "yard2: Can't create socket !");
		return 0;
	}

	if (connect(drv.fd, (struct sockaddr *)&srvAddr, srvAddrLen) < 0) {
		logprintf(LIRC_ERROR, "yard2: Can't connect to yardsrv !");
		return 0;
	}

/* not used in yard2	
	// Register IR code notification
	if ( !yard_sendSrvCmd(SRVCMD_IRREG) )
	{
		logprintf(LIRC_ERROR, "yard2: Can't register IR code notification !");
		return 0;
	}
*/
	return 1;
}

static int yard2_deinit(void)
{
	// Unregister IR code notification
	// not used in yard2 yard_sendSrvCmd(SRVCMD_IRUNREG);

	close(drv.fd);
	drv.fd = -1;
	return 1;
}

static int yard2_send(struct ir_remote *remote, struct ir_ncode *icode)
{
	unsigned long long sendir;
	unsigned char buffer[8];

	//Error check
	if (drv.fd < 0) {
		return 0;
	}

	sendir = icode->code;
	LOGPRINTF(1, "SEND IR-Code: %llx", sendir);

	buffer[0] = 0x81;	//Send IR command ID
	buffer[1] = 255 - buffer[0];
	//  example  0715000c0000  
	buffer[2] = (sendir & 0x0000FF0000000000) >> 40;
	buffer[3] = (sendir & 0x000000FF00000000) >> 32;
	buffer[4] = (sendir & 0x00000000FF000000) >> 24;
	buffer[5] = (sendir & 0x0000000000FF0000) >> 16;
	buffer[6] = (sendir & 0x000000000000FF00) >> 8;
	buffer[7] = (sendir & 0x00000000000000FF);

	send(drv.fd, buffer, 8, MSG_NOSIGNAL);
	return 1;
}

static char *yard2_rec(struct ir_remote *remotes)
{
	YARD_IRCODE yardIrCode;
	char* m;
	int byteCnt;
	int i;

	// Error check
	if (drv.fd < 0) {
		return 0;
	}

	last = end;
	gettimeofday(&start, NULL);

	// Receive IR code from YARD server
	byteCnt = read(drv.fd,
		       (unsigned char *)&yardIrCode, sizeof(YARD_IRCODE));
	LOGPRINTF(1, "yard2: received %d bytes !", byteCnt);
	if (byteCnt < sizeof(YARD_IRCODE)) {
		logprintf(LIRC_ERROR,
			  "yard2: Expected %d bytes - received %d bytes !",
			  sizeof(YARD_IRCODE), byteCnt);
		return NULL;
	}

	gettimeofday(&end, NULL);

	// Extract IR code bytes
	code = 0;
	for (i = 0; i < IRCODE_NUM_BYTES; i++) {
		code <<= 8;
		code |= yardIrCode[i];	//.abIrData[i]; //
	}
	LOGPRINTF(1, "Receive IR-Code: %llx", (unsigned long long)code);
	m = decode_all(remotes);
	return m;
}

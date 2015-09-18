/*
 * Remote control driver for the Creative iNFRA CDrom
 *
 *  by Leonid Froenchenko <lfroen@il.marvell.com>
 *    thnx Kira Langerman for donated hardware
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
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

#include <scsi/sg.h>
#include <scsi/scsi.h>

#include "lirc_driver.h"



#define MAX_SCSI_REPLY_LEN     96
#define SCSI_INQ_CMD_LEN       6
#define SCSI_TUR_CMD_LEN       6
#define SCSI_SEN_CMD_LEN       10


static const logchannel_t logchannel = LOG_DRIVER;

static int creative_infracd_init(void);
static int creative_infracd_deinit(void);
static int creative_infracd_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static char* creative_infracd_rec(struct ir_remote* remotes);

/* private stuff */
#define MASK_COMMAND_PRESENT 0x00f00000

static int test_device_command(int fd);


const struct driver hw_creative_infracd = {
	.name		= "creative_infracd",
	.device		= 0,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 8,
	.init_func	= creative_infracd_init,
	.deinit_func	= creative_infracd_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= creative_infracd_rec,
	.decode_func	= creative_infracd_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/sg*",
};

const struct driver* hardwares[] = { &hw_creative_infracd, (const struct driver*)NULL };


/*
 * opened /dev/sg<x>. I'm not using drv.fd for reasons of lirc design
 */
static int int_fd = 0;

/* last code seen from remote */
static ir_code code;

static char dev_name[32];

int is_my_device(int fd, const char* name)
{
	sg_io_hdr_t io_hdr;
	int k;
	unsigned char inqCmdBlk[SCSI_INQ_CMD_LEN] = { INQUIRY, 0, 0, 0, MAX_SCSI_REPLY_LEN, 0 };
	char Buff[MAX_SCSI_REPLY_LEN];
	unsigned char sense_buffer[32];

	/* Just to be safe, check we have a sg device wigh version > 3 */
	if ((ioctl(fd, SG_GET_VERSION_NUM, &k) < 0) || (k < 30000)) {
		log_trace("%s isn't sg device version > 3", name);
		return 0;
	}
	usleep(10);
	log_trace("%s is valid sg device - checking what it is", name);

	/* Prepare INQUIRY command */
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof(inqCmdBlk);
	io_hdr.mx_sb_len = sizeof(sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = MAX_SCSI_REPLY_LEN;
	io_hdr.dxferp = Buff;
	io_hdr.cmdp = inqCmdBlk;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = 2000;

	if (ioctl(fd, SG_IO, &io_hdr) < 0) {
		log_error("INQUIRY SG_IO ioctl error");
		return 0;
	}
	usleep(10);
	if ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
		log_error("INQUIRY: SCSI status=0x%x host_status=0x%x driver_status=0x%x", io_hdr.status,
			  io_hdr.host_status, io_hdr.driver_status);
		return 0;
	}
	/* check INQUIRY returned string */
	if (strncmp(Buff + 8, "CREATIVE", 8) > 0)
		log_error("%s is %s (vendor isn't Creative)", name, Buff + 8);

	/* now run sense_mode_10 for page 0 to see if this is really it */
	if (test_device_command(fd) < 0)
		return 0;
	return 1;
}

/* actually polling function */
int test_device_command(int fd)
{
	sg_io_hdr_t io_hdr;
	unsigned char senCmdBlk[SCSI_SEN_CMD_LEN] = { MODE_SENSE_10, 0, 0, 0, 0, 0, 0, 0, MAX_SCSI_REPLY_LEN, 0 };

	unsigned char sense_buffer[255];
	unsigned char Buff[MAX_SCSI_REPLY_LEN];
	unsigned int* i_Buff = (unsigned int*)Buff;

	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof(senCmdBlk);
	io_hdr.mx_sb_len = sizeof(sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_TO_FROM_DEV;
	io_hdr.dxfer_len = MAX_SCSI_REPLY_LEN;
	io_hdr.dxferp = Buff;
	io_hdr.cmdp = senCmdBlk;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = 2000;

	memset(Buff, 0, MAX_SCSI_REPLY_LEN);

	if (ioctl(fd, SG_IO, &io_hdr) < 0) {
		log_trace("MODE_SENSE_10 SG_IO ioctl error");
		return -1;
	}

	if ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
		log_trace("MODE_SENSE_10: status=0x%x host=0x%x driver=0x%x", io_hdr.status,
			  io_hdr.host_status, io_hdr.driver_status);
		return -1;
	}
	if (i_Buff[2] & MASK_COMMAND_PRESENT)
		// when command present - opcode is found on bits [15:8]
		return (i_Buff[3] >> 8) & 0xff;
	// device ok, but no command
	return 0;
}

char* creative_infracd_rec(struct ir_remote* remotes)
{
	int cmd;

	while ((cmd = test_device_command(int_fd)) == 0)
		usleep(40);
	;
	if (cmd == -1)
		return 0;

	code = (reverse(cmd, 8) << 8) | (~reverse(cmd, 8) & 0xff);
	return decode_all(remotes);
}

int creative_infracd_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 16, 0x8435, 16, code, 0, 0))
		return 0;

	return 1;
}

int init_device(void)
{
	char c;
	int fd;

	/* user overriding autoprobing */
	if (drv.device) {
		fd = open(drv.device, O_RDWR);
		if (fd < 0) {
			log_trace("Init: open of %s failed", drv.device);
			return 0;
		}
		/* open ok, test device */
		if (is_my_device(fd, drv.device))
			return fd;
		return 0;
	}
	for (c = 'a'; c < 'z'; c++) {
		sprintf(dev_name, "/dev/sg%c", c);
		fd = open(dev_name, O_RDWR);
		if (fd < 0) {
			log_trace("Probing: open of %s failed", dev_name);
			continue;
		}
		/* open ok, test device */
		if (is_my_device(fd, dev_name)) {
			drv.device = dev_name;
			return fd;
		}
	}
	return 0;
}

int creative_infracd_init(void)
{
	int fd;

	log_trace("Creative iNFRA driver: begin search for device");

	fd = init_device();
	if (fd) {
		/*
		 * lircd making "select" for device we open. However,
		 * /dev/sg<x> does not behave like /dev/ttyS<x>, i.e. it
		 * never asserted untill we explicitly send some scsi
		 * command. So, make lircd think that device always
		 * has data, and make polling loops myself
		 */
		drv.fd = open("/dev/null", O_RDONLY);
		if (drv.fd == -1) {
			close(fd);
			return 0;
		}
		int_fd = fd;
		log_trace("Probing: %s is my device", drv.device);
		return 1;
	}

	/* probing failed - simple sanity check why */
	fd = open("/proc/scsi/scsi", O_RDONLY);
	if (fd < 0) {
		log_trace("Probing: unable to open /proc/scsi/scsi");
	} else {
		close(fd);
		fd = open("/proc/scsi/ide-scsi/0", O_RDONLY);
		if (fd < 0) {
			log_trace("Probing: scsi support present but ide-scsi is not loaded");
		} else {
			close(fd);
			log_trace(
				  "Probing: scsi in kernel, ide-scsi is loaded. Bad configuration or "
				  "device not present");
		}
	}
	return 0;
}

int creative_infracd_deinit(void)
{
	close(drv.fd);
	close(int_fd);
	return 1;
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

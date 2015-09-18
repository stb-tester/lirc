/*
 * hw_livedrive_seq.c - lirc routines for a Creative Labs LiveDrive.
 *
 *     Copyright (C) 2003 Stephen Beahm <stephenbeahm@adelphia.net>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301 USA.
 */

#include "lirc_driver.h"

#include "livedrive_common.h"

static const logchannel_t logchannel = LOG_DRIVER;

char* livedrive_rec_seq(struct ir_remote* remotes)
{
	int i;
	struct sequencer_packet seq;
	struct midi_packet midi;
	unsigned char* bytep = (unsigned char*)&midi;
	ir_code bit[4];

	last = end;

	gettimeofday(&start, NULL);
	/* poll for system exclusive status byte so we don't try to
	 * record other midi events */
	do
		chk_read(drv.fd, &seq, sizeof(seq));
	while (seq.data != SYSEX);

	for (i = 0; i < sizeof(midi); i++) {
		chk_read(drv.fd, &seq, sizeof(seq));
		/* skip 2 missing filler bytes for audigy2 non-infrared messages */
		if (midi.dev == NONREMOTE && i == 4)
			i += 2;
		*(bytep + i) = seq.data;
	}
	gettimeofday(&end, NULL);

	/* test for correct system exclusive end byte so we don't try
	 * to record other midi events */
	if (midi.sysex_end != SYSEX_END)
		return NULL;

	bit[0] = (midi.keygroup >> 3) & 0x1;
	bit[1] = (midi.keygroup >> 2) & 0x1;
	bit[2] = (midi.keygroup >> 1) & 0x1;
	bit[3] = (midi.keygroup >> 0) & 0x1;

	pre = reverse(midi.remote[0] | (midi.remote[1] << 8), 16) | (bit[0] << 8) | bit[1];
	code = reverse(midi.key[0] | (midi.key[1] << 8), 16) | (bit[2] << 8) | bit[3];

	return decode_all(remotes);
}

struct driver hw_livedrive_seq = {
	.name		= "livedrive_seq",
	.device		= "/dev/sequencer",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= livedrive_init,
	.deinit_func	= livedrive_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= livedrive_rec_seq,
	.decode_func	= livedrive_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/sequencer",
};

const struct driver* hardwares[] = { &hw_livedrive_seq, (struct driver*)NULL };

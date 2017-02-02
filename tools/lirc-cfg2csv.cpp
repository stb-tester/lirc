/****************************************************************************
** lirc-lsremotes **********************************************************
****************************************************************************
*
* lirc-lsremotes - list remotes from the remotes database.
*
*/

#include <config.h>

#include <dirent.h>
#include <getopt.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lirc_private.h"
#include "lirc_client.h"

#define TXBUFSZ         65536

static const logchannel_t logchannel = LOG_APP;


static const char* const USAGE =
	"Convert lirc remote controls to CSV.\n"
	"Synopsis:\n"
	"    lirc-cfg2csv <path>\n"
	"    lirc-cfg2csv [-h | -v]\n"
	"\n"
	"<path> is the path to the remotes directory or a single file.\n"
	"\n"
	"Options:\n"
	"    -v, --version      Print version.\n"
	"    -h, --help         Print this message.\n"
	"\n"
	"This can be useful for investigating the effect of modifying\n"
	"parameters in your lirc config files on what will be written out\n";


static struct option options[] = {
	{ "help",    no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'v' },
	{ 0,	     0,		  0,	0   }
};

static int print_remote_csv(struct ir_remote* remote);

/** Print a line for each remote definition in lircd.conf file on path. */
void print_remotes(const char* path)
{
	char my_path[256];
	struct ir_remote* r;
	FILE* f;
	const char* dir;

	strncpy(my_path, path, sizeof(my_path));
	dir = dirname(my_path);
	if (strrchr(dir, '/') != NULL)
		dir = strrchr(dir, '/') + 1;
	f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open %s (!)\n", path);
		return;
	}
	r = read_config(f, path);
	while (r != NULL && r != (void*)-1) {
		print_remote_csv(r);
		r = r->next;
	};
	fclose(f);
}

static int modulate_pulses(unsigned char* buf, size_t size,
	const int* pulseptr, int n_pulses, uint32_t f_sample, uint32_t f_carrier,
	unsigned int duty_cycle)
{
	uint32_t div_carrier;
	uint32_t duty;
	lirc_t pulse;
	int pulsewidth;
	int val_carrier;
	unsigned bufidx;
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
		pulsewidth =
		    ((uint64_t)f_sample) * ((uint32_t)(pulse & PULSE_MASK))
			/ 1000000ul;

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
			val_carrier = div_carrier < duty ? 1 : 0;

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

static void print_data(unsigned char* data, size_t len)
{
	size_t n;
	for (n=0; n<len; n++)
		printf(data[n] ? "1, " : "0, ");
	printf("\n");
}

static int print_remote_csv(struct ir_remote* remote)
{
	unsigned char buf[TXBUFSZ];
	ssize_t buf_len, max_len = 0;
	uint32_t f_carrier = remote->freq == 0 ? DEFAULT_FREQ : remote->freq;

	/* A sample rate of carrier*2 means we will get the pattern 1010101010
	 * when the blaster is on and 0000000000 when it is off. */
	uint32_t f_sample = f_carrier * 2;

	const lirc_t* pulseptr;
	int n_pulses;

	for (struct ir_ncode* code = remote->codes; code->name; code++) {
		/* initialize decoded buffer: */
		if (!send_buffer_put(remote, code))
			return -1;

		/* init vars: */
		n_pulses = send_buffer_length();
		pulseptr = send_buffer_data();

		buf_len = modulate_pulses(
			buf, sizeof(buf), pulseptr, n_pulses, f_sample,
			f_carrier, 50);
		if (buf_len > max_len)
			max_len = buf_len;
	}

	printf("\t");
	for (unsigned n = 0; n < max_len; n++)
		printf("%i,", (int)(n * 1000000 / f_sample));
	printf("\n");

	for (struct ir_ncode* code = remote->codes; code->name; code++) {
		/* initialize decoded buffer: */
		if (!send_buffer_put(remote, code))
			return -1;

		/* init vars: */
		n_pulses = send_buffer_length();
		pulseptr = send_buffer_data();

		buf_len = modulate_pulses(buf, sizeof(buf), pulseptr, n_pulses, f_sample,
					  f_carrier, 50);

		printf("%s, ", code->name);
		print_data(buf, buf_len);

		fflush(stdout);
	}

	return 0;
}


int main(int argc, char** argv)
{
	const char* dirpath;
	int c;

	while ((c = getopt_long(argc, argv, "shdv", options, NULL)) != EOF) {
		switch (c) {
		case 'h':
			puts(USAGE);
			return EXIT_SUCCESS;
		case 'v':
			printf("%s\n", "lirc-lsremotes " VERSION);
			return EXIT_SUCCESS;
		case '?':
			fprintf(stderr, "unrecognized option: -%c\n", optopt);
			fputs("Try `lirc-lsremotes -h' for more information.\n",
			      stderr);
			return EXIT_FAILURE;
		}
	}
	if (argc == optind + 1) {
		dirpath = argv[optind];
	} else {
		fputs(USAGE, stderr);
		return EXIT_FAILURE;
	}
	print_remotes(dirpath);
	return 0;
}

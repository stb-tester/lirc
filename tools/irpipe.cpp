#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef HAVE_KERNEL_LIRC_H
#include <linux/lirc.h>
#else
#include "media/lirc.h"
#endif

#ifndef __u32
#define __u32 uint32_t
#endif

#include "irpipe.h"


static const char* const help =
	"Write data to irpipe kernel device, applying conversions\n"
	"and ioctls.\n\n"
	"Usage:  irpipe [options] <file\n"
	"        irpipe --read [options] >file\n"
	"        irpipe --filter [options] <infile >outfile\n\n"
	"Options:\n"
	"\t -F --features=bitmask\tSet kernel device features set\n"
	"\t -l --length=bits\tSet kernel device signal length\n"
	"\t -d --device=driver\tSet kernel device, default /dev/irpipe0\n"
	"\t -b --bin2text\t\tConvert binary data to text\n"
	"\t -t --text2bin\t\tConvert text data to binary\n"
	"\t -r --read\t\tSend data from kernel device to stdout\n"
	"\t -f --filter\t\tSend data from stdin to stdout\n"
	"\t -B --bits=bits\t\tConvert bits wide LIRCCODE data\n"
	"\t -s --add-sync\t\tAdd long initial sync on converted output\n"
	"\t -h --help\t\tDisplay this message\n"
	"\t -v --version\t\tDisplay version\n";


static const struct option options[] = {
	{ "features",	    required_argument, NULL, 'F' },
	{ "length",	    required_argument, NULL, 'l' },
	{ "bin2text",	    no_argument,       NULL, 't' },
	{ "text2bin",	    no_argument,       NULL, 'b' },
	{ "bits",	    required_argument, NULL, 'B' },
	{ "write",	    no_argument,       NULL, 'w' },
	{ "filter",	    no_argument,       NULL, 'f' },
	{ "add-sync",	    no_argument,       NULL, 's' },
	{ "device",	    required_argument, NULL, 'd' },
	{ "help",	    no_argument,       NULL, 'h' },
	{ "version",	    no_argument,       NULL, 'v' },
	{ 0,		    0,		       0,    0	 }
};

static enum {MODE_READ, MODE_WRITE, MODE_FILTER} opt_mode = MODE_WRITE;
static const char* opt_device = "/dev/irpipe0";
static unsigned long opt_features = LONG_MAX;
static int opt_length = -1;
static bool opt_totext = false;
static bool opt_tobin = false;
static bool opt_add_sync = false;
static int opt_bits = 0;


static void parse_options(int argc, char* argv[])
{

	int c;
	long l;
	const char* const optstring = "F:l:bB:trfd:hsv";

	while (1) {
		c = getopt_long(argc, argv, optstring, options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'F':
			l = strtol(optarg, NULL, 10);
			if (l >= INT_MAX || l <= INT_MIN) {
				fprintf(stderr, "Bad numeric: %s\n", optarg);
				exit(1);
			}
			opt_features = (int)l;
		case 'l':
			l = strtol(optarg, NULL, 10);
			if (l >= INT_MAX || l <= INT_MIN) {
				fprintf(stderr, "Bad numeric: %s\n", optarg);
				exit(1);
			}
			opt_length = l;
		case 'B':
			l = strtol(optarg, NULL, 10);
			if (l >= INT_MAX || l <= INT_MIN) {
				fprintf(stderr, "Bad numeric: %s\n", optarg);
				exit(1);
			}
			opt_bits = l;
		case 'b':
			opt_tobin = true;
			break;
		case 's':
			opt_add_sync = true;
			break;
		case 't':
			opt_totext = true;
			break;
		case 'r':
			opt_mode = MODE_READ;
			break;
		case 'f':
			opt_mode = MODE_FILTER;
			break;
		case 'd':
			opt_device = strdup(optarg);
			break;
		case 'h':
			fputs(help, stdout);
			exit(0);
		case 'v':
			fputs("irpipe " VERSION "\n", stdout);
			exit(0);
		default:
			return;
		}
	}
}


/** Redirect newfd to the kernel irpipe device using mode. */
static void irpipe_setup(int newfd, const char* device, int mode)
{
	int fd;

	fd = open(device, mode);
	if (fd == -1) {
		perror("Cannot open kernel device");
		exit(1);
	}
	fd = dup2(fd, newfd);
	if (fd == -1) {
		perror("Cannot dup2 kernel device");
		exit(1);
	}
}


/** Parse a pulse/space text line, return duration. */
static uint32_t process_line(const char* token1, const char* token2)
{
	long value;

	if (token1 == NULL || token2 == NULL)
		return (uint32_t)-1;
	value = strtol(token2, NULL, 10);
	if (value == LONG_MIN || value >= PULSE_BIT || value == 0)
		return (uint32_t)-1;
	if (strcmp("pulse", token1) == 0)
		return value |= PULSE_BIT;
	else if (strcmp("space", token1) == 0)
		return (uint32_t)value;
	else if (strcmp("code", token1) == 0)
		return (uint32_t)value;
	return (uint32_t)-1;
}


/** Copy stdin to stdout, converting ascii data to binary. */
static void write_tobin(void)
{
	char line[128];
	char buff[128];
	char* token1;
	char* token2;
	int32_t value;
	const int len = opt_bits == 0 ? sizeof(uint32_t) : opt_bits/8;

	if (opt_add_sync) {
		value = 1000000;
		if (write(1, &value, sizeof(uint32_t)) != sizeof(uint32_t))
			perror("write() failed.");
	}
	while (fgets(line, sizeof(line), stdin) != NULL) {
		strncpy(buff, line, sizeof(buff) - 1);
		token1 = strtok(buff, "\n ");
		token2 = strtok(NULL, "\n ");
		value = process_line(token1, token2);
		if (value == -1) {
			fprintf(stderr,
				"Illegal data (ignored):\"%s\"\n", line);
		} else {
			if (write(1, &value,len) != len)
				perror("write() failed");
		}
	}
}


/** Copy stdin to stdout, converting binary data to text. */
static void write_totext(void)
{
	char buff[2048 * sizeof(int)];
	int r;
	int i;
	const uint32_t* data;

	if (opt_add_sync)
		puts("space 1000000\n");
	while ((r = read(0, buff, sizeof(buff))) > 0) {
		for (i = 0; i < r; i += sizeof(int)) {
			data = (uint32_t*)(&buff[i]);
			printf("%s %u\n",
			       (*data & PULSE_BIT) ? "pulse" : "space",
			       *data & PULSE_MASK);
		}
	}
	if (r == -1) {
		perror("irpipe: Cannot read stdin");
		exit(1);
	}
}


/** Send verbatim copy of stdin to stdout. */
static void write_raw(void)
{
	char buff[2048 * sizeof(int)];
	int r;
	int written;

	while ((r = read(0, buff, sizeof(buff))) > 0) {
		written = write(1, buff, r);
		if (written != r) {
			perror("irpipe: Cannot write in raw copy");
			exit(1);
		}
	}
	if (r == -1) {
		perror("irpipe: Cannot read stdin");
		exit(1);
	}
}


static void run_ioctl(int cmd, unsigned long arg)
{
	int fd = (opt_mode == MODE_WRITE) ? 1 : 0;
	int r = ioctl(fd, cmd, arg);
	if (r == -1) {
		perror("Cannot run ioctl");
		exit(1);
	}
}


int main(int argc, char**argv)
{
	parse_options(argc, argv);
	if (opt_mode == MODE_WRITE)
		irpipe_setup(1, opt_device, O_WRONLY);
	else if (opt_mode == MODE_READ)
		irpipe_setup(0, opt_device, O_RDONLY);
	if (opt_features != LONG_MAX)
		run_ioctl(LIRC_SET_FEATURES, opt_features);
	if (opt_length != -1)
		run_ioctl(LIRC_SET_LENGTH, opt_length);
	if (opt_bits > 0 && opt_mode != MODE_FILTER) {
		run_ioctl(LIRC_SET_REC_MODE, LIRC_MODE_LIRCCODE);
		run_ioctl(LIRC_SET_SEND_MODE, LIRC_MODE_LIRCCODE);
	}
	if (opt_tobin)
		write_tobin();
	else if (opt_totext)
		write_totext();
	else
		write_raw();
}

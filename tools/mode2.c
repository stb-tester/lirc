
/****************************************************************************
 ** mode2.c *****************************************************************
 ****************************************************************************
 *
 * mode2 - shows the pulse/space length of a remote button
 *
 * Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
 * Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _CYGWIN_
#define __USE_LINUX_IOCTL_DEFS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include "lirc_private.h"
static char *device = NULL;
static int dmode = 0;
static int t_div = 500;
static int gap = 10000;
static int use_raw_access = 0;


static const char* const help =
"Usage: mode2 [options]\n"
"\t -d --device=device\tread from given device\n"
"\t -H --driver=driver\tuse given driver\n"
"\t -U --plugindir=path\tLoad plugins from path.\n"
"\t -m --mode\t\tenable column display mode\n"
"\t -r --raw\t\taccess device directly\n"
"\t -g --gap=time\t\ttreat spaces longer than time as the gap\n"
"\t -s --scope=time\tenable 'scope like display with time us per char.\n"
"\t -h --help\t\tdisplay usage summary\n"
"\t -v --version\t\tdisplay version\n";


static const struct option options[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{"device", required_argument, NULL, 'd'},
	{"driver", required_argument, NULL, 'H'},
	{"mode", no_argument, NULL, 'm'},
	{"raw", no_argument, NULL, 'r'},
	{"gap", required_argument, NULL, 'g'},
	{"scope", required_argument, NULL, 's'},
	{"plugindir", required_argument, NULL, 'U'},
	{0, 0, 0, 0}
};


static void add_defaults(void)
{
	char level[4];
	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());

	const char* const defaults[] = {
		"mode2:driver", 	"default",
		"mode2:lircdfile", 	LIRCD,
		"lircd:logfile", 	"syslog",
		"lircd:debug", 		level,
		"lircd:plugindir", 	PLUGINDIR,
		"lircd:configfile",  	LIRCDCFGFILE,
		(const char*)NULL, 	(const char*)NULL
	};
	options_add_defaults(defaults);
}


static void parse_options(int argc, char** argv)
{
	int c;
	static const char* const optstring= "hvd:H:mrg:s:U:";
	char driver[64];

	strcpy(driver, "default");

	add_defaults();
	while ((c = getopt_long(argc, argv, optstring, options, NULL)) != -1)
	{
		switch (c) {
		case 'h':
			printf(help);
			exit(EXIT_SUCCESS);
		case 'H':
			strncpy(driver, optarg, sizeof(driver) - 1);
			break;
		case 'v':
			printf("%s %s\n", progname, VERSION);
			exit(EXIT_SUCCESS);
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 's':
			dmode = 2;
			t_div = atoi(optarg);
			break;
		case 'm':
			dmode = 1;
			break;
		case 'r':
			use_raw_access = 1;
			break;
		case 'g':
			gap = atoi(optarg);
			break;
		default:
			printf("Usage: %s [options]\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", progname);
		exit(EXIT_FAILURE);
	}
	if (hw_choose_driver(driver) != 0) {
		fprintf(stderr, "Driver `%s' not found.", driver);
		fputs(" (Missing -U/--plugins option?)\n", stderr);
		fputs("Available drivers:\n", stderr);
		hw_print_drivers(stderr);
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char **argv)
{
	int fd;
	char buffer[sizeof(ir_code)];
	lirc_t data;
	__u32 mode;
	/*
	 * Was hard coded to 50000 but this is too long, the shortest gap in the
	 * supplied .conf files is 10826, the longest space defined for any one,
	 * zero or header is 7590
	 */
	struct stat s;
	__u32 code_length;
	size_t count = sizeof(lirc_t);
	int i;
        char path[128];

	lirc_log_get_clientlog("mode2", path, sizeof(path));
	lirc_log_set_file(path);
	lirc_log_open("mode2", 1, LIRC_NOTICE);

	strncpy(progname, "mode2", sizeof(progname));
	hw_choose_driver(NULL);
	options_load(argc, argv, NULL, parse_options);
	if (strcmp(device, LIRCD) == 0) {
		fprintf(stderr, "%s: refusing to connect to lircd socket\n", progname);
		return EXIT_FAILURE;
	}

	if (use_raw_access) {
		fd = open(device, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "%s: error opening %s\n", progname, device);
			perror(progname);
			exit(EXIT_FAILURE);
		};

		if ((fstat(fd, &s) != -1) && (S_ISFIFO(s.st_mode))) {
			/* can't do ioctls on a pipe */
		} else if ((fstat(fd, &s) != -1) && (!S_ISCHR(s.st_mode))) {
			fprintf(stderr,
				"%s: %s is not a character device\n",
				progname, device);
			fprintf(stderr,
				"%s: use the -d option to specify the"
					" correct device\n",
				progname);
			close(fd);
			exit(EXIT_FAILURE);
		} else if (ioctl(fd, LIRC_GET_REC_MODE, &mode) == -1) {
			printf("This program is only intended for receivers"
			       " supporting the pulse/space layer.\n");
			printf("Note that this is no error, but this program "
			       "simply makes no sense for your\n" "receiver.\n");
			printf("In order to test your setup run lircd with "
			       "the --nodaemon option and \n then check if the"
			       " remote works with the irw tool.\n");
			close(fd);
			exit(EXIT_FAILURE);
		}
	} else {
		curr_driver->open_func(device);
		if (!curr_driver->init_func || !curr_driver->init_func()) {
			return EXIT_FAILURE;
		}
		fd = curr_driver->fd;	/* please compiler */
		mode = curr_driver->rec_mode;
		if (mode != LIRC_MODE_MODE2) {
			if (strcmp(curr_driver->name, "default") == 0) {
				printf("Please use the --raw option to access "
				       "the device directly instead through\n"
				       "the abstraction layer.\n");
			} else {
				printf("This program does not work for this"
 				       " hardware yet\n");
			}
			exit(EXIT_FAILURE);
		}

	}

	if (mode == LIRC_MODE_LIRCCODE) {
		if (use_raw_access) {
			if (ioctl(fd, LIRC_GET_LENGTH, &code_length) == -1) {
				fprintf(stderr,
					"%s: could not get code length\n",
					progname);
				perror(progname);
				close(fd);
				exit(EXIT_FAILURE);
			}
		} else {
			code_length = curr_driver->code_length;
		}
		if (code_length > sizeof(ir_code) * CHAR_BIT) {
			fprintf(stderr,
				"%s: cannot handle %u bit codes\n",
				progname, code_length);
			close(fd);
			exit(EXIT_FAILURE);
		}
		count = (code_length + CHAR_BIT - 1) / CHAR_BIT;
	}
	while (1) {
		int result;

		if (use_raw_access) {
			result = read(fd,
				      (mode == LIRC_MODE_MODE2 ?
						(void *)&data : buffer),
				      count);
			if (result != count) {
				fprintf(stderr, "read() failed\n");
				break;
			}
		} else {
			if (mode == LIRC_MODE_MODE2) {
				data = curr_driver->readdata(0);
				if (data == 0) {
					fprintf(stderr,
					        "readdata() failed\n");
					break;
				}
			} else {
				/* not implemented yet */
			}
		}

		if (mode != LIRC_MODE_MODE2) {
			printf("code: 0x");
			for (i = 0; i < count; i++) {
				printf("%02x", (unsigned char)buffer[i]);
			}
			printf("\n");
			fflush(stdout);
			continue;
		}

		switch (dmode) {
		case 0:
			printf("%s %u\n", (
			       data & PULSE_BIT) ? "pulse" : "space",
			       (__u32) (data & PULSE_MASK));
			break;
		case 1: {
			static int bitno = 1;

			/* print output like irrecord raw config file data */
			printf(" %8u", (__u32) data & PULSE_MASK);
			++bitno;
			if (data & PULSE_BIT) {
				if ((bitno & 1) == 0) {
					/* not in expected order */
					printf("-pulse");
				}
			} else {
				if (bitno & 1) {
					/* not in expected order */
					printf("-space");
				}
				if (((data & PULSE_MASK) > gap) || (bitno >= 6)) {
					/* real long space or more
					   than 6 codes, start new line */
					printf("\n");
					if ((data & PULSE_MASK) > gap)
						printf("\n");
					bitno = 0;
				}
			}
			break;
		}
		case 2:
			if ((data & PULSE_MASK) > gap)
				printf("_\n\n_");
			else
				printf("%.*s",
				       ((data & PULSE_MASK) + t_div/2) / t_div,
				       (data & PULSE_BIT) ?
						"------------" : "____________");
			break;
}
		fflush(stdout);
	};
	return (EXIT_SUCCESS);
}

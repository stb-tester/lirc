
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
	int gap = 10000;
	int t_div = 500;
	char *device = LIRC_DRIVER_DEVICE;
	struct stat s;
	int dmode = 0;
	__u32 code_length;
	size_t count = sizeof(lirc_t);
	int i;
	int use_raw_access = 0;
	int have_device = 0;

	strncpy(progname, "mode2", sizeof(progname));
	hw_choose_driver(NULL);
	while (1) {
		int c;
		static struct option long_options[] = {
			{"help", no_argument, NULL, 'h'},
			{"version", no_argument, NULL, 'v'},
			{"device", required_argument, NULL, 'd'},
			{"driver", required_argument, NULL, 'H'},
			{"mode", no_argument, NULL, 'm'},
			{"raw", no_argument, NULL, 'r'},
			{"gap", required_argument, NULL, 'g'},
			{"scope", required_argument, NULL, 's'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv, "hvd:H:mrg:s:", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			printf("Usage: %s [options]\n", progname);
			printf("\t -h --help\t\tdisplay usage summary\n");
			printf("\t -v --version\t\tdisplay version\n");
			printf("\t -d --device=device\tread from given device\n");
			printf("\t -H --driver=driver\tuse given driver\n");
			printf("\t -m --mode\t\tenable column display mode\n");
			printf("\t -r --raw\t\taccess device directly\n");
			printf("\t -g --gap=time\t\ttreat spaces longer than time as the gap\n");
			printf("\t -s --scope=time\tenable 'scope like display with time us per char.\n");
			return (EXIT_SUCCESS);
		case 'H':
			if (hw_choose_driver(optarg) != 0) {
				fprintf(stderr, "Driver `%s' not supported.\n", optarg);
				hw_print_drivers(stderr);
				exit(EXIT_FAILURE);
			}
			break;
		case 'v':
			printf("%s %s\n", progname, VERSION);
			return (EXIT_SUCCESS);
		case 'd':
			device = optarg;
			have_device = 1;
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
			return (EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "%s: too many arguments\n", progname);
		return (EXIT_FAILURE);
	}
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
			fprintf(stderr, "%s: %s is not a character device\n", progname, device);
			fprintf(stderr, "%s: use the -d option to specify the correct device\n", progname);
			close(fd);
			exit(EXIT_FAILURE);
		} else if (ioctl(fd, LIRC_GET_REC_MODE, &mode) == -1) {
			printf("This program is only intended for receivers supporting the pulse/space layer.\n");
			printf("Note that this is no error, but this program "
			       "simply makes no sense for your\n" "receiver.\n");
			printf("In order to test your setup run lircd with "
			       "the --nodaemon option and \n" "then check if the remote works with the irw tool.\n");
			close(fd);
			exit(EXIT_FAILURE);
		}
	} else {
		if (have_device)
			hw.device = device;
		if (!hw.init_func || !hw.init_func()) {
			return EXIT_FAILURE;
		}
		fd = hw.fd;	/* please compiler */
		mode = hw.rec_mode;
		if (mode != LIRC_MODE_MODE2) {
			if (strcmp(hw.name, "default") == 0) {
				printf("Please use the --raw option to access "
				       "the device directly instead through\n" "the abstraction layer.\n");
			} else {
				printf("This program does not work for this hardware yet\n");
			}
			exit(EXIT_FAILURE);
		}

	}

	if (mode == LIRC_MODE_LIRCCODE) {
		if (use_raw_access) {
			if (ioctl(fd, LIRC_GET_LENGTH, &code_length) == -1) {
				fprintf(stderr, "%s: could not get code length\n", progname);
				perror(progname);
				close(fd);
				exit(EXIT_FAILURE);
			}
		} else {
			code_length = hw.code_length;
		}
		if (code_length > sizeof(ir_code) * CHAR_BIT) {
			fprintf(stderr, "%s: cannot handle %u bit codes\n", progname, code_length);
			close(fd);
			exit(EXIT_FAILURE);
		}
		count = (code_length + CHAR_BIT - 1) / CHAR_BIT;
	}
	while (1) {
		int result;

		if (use_raw_access) {
			result = read(fd, (mode == LIRC_MODE_MODE2 ? (void *)&data : buffer), count);
			if (result != count) {
				fprintf(stderr, "read() failed\n");
				break;
			}
		} else {
			if (mode == LIRC_MODE_MODE2) {
				data = hw.readdata(0);
				if (data == 0) {
					fprintf(stderr, "readdata() failed\n");
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
			printf("%s %u\n", (data & PULSE_BIT) ? "pulse" : "space", (__u32) (data & PULSE_MASK));
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
				printf("%.*s", ((data & PULSE_MASK) + t_div / 2) / t_div, (data & PULSE_BIT) ? "------------" : "____________");
			break;
}
		fflush(stdout);
	};
	return (EXIT_SUCCESS);
}

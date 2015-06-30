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
#include <pwd.h>

#include "lirc_private.h"

static char* opt_device = NULL;
static int opt_dmode = 0;
static int t_div = 500;
static unsigned int opt_gap = 10000;
static int opt_raw_access = 0;

static const char* const help =
	"Usage: mode2 [options]\n"
	"\t -d --device=device\tread from given device\n"
	"\t -H --driver=driver\tuse given driver\n"
	"\t -U --plugindir=path\tLoad plugins from path.\n"
	"\t -k --keep-root\t\tkeep root privileges\n"
	"\t -m --mode\t\tenable column display mode\n"
	"\t -r --raw\t\taccess device directly\n"
	"\t -g --gap=time\t\ttreat spaces longer than time as the gap\n"
	"\t -s --scope=time\t'scope like display with time us per char.\n"
	"\t -h --help\t\tdisplay usage summary\n"
	"\t -v --version\t\tdisplay version\n"
	"\t -A --driver-options=key:value[|key:value...]\n"
	"\t\t\t\tSet driver options\n";


static const struct option options[] = {
	{"help",           no_argument,       NULL, 'h'},
	{"version",        no_argument,       NULL, 'v'},
	{"device",         required_argument, NULL, 'd'},
	{"driver",         required_argument, NULL, 'H'},
	{"keep-root",      no_argument,       NULL, 'k'},
	{"mode",           no_argument,       NULL, 'm'},
	{"raw",            no_argument,       NULL, 'r'},
	{"gap",            required_argument, NULL, 'g'},
	{"scope",          required_argument, NULL, 's'},
	{"plugindir",      required_argument, NULL, 'U'},
	{"driver-options", required_argument, NULL, 'A'},
	{0,	           0,		      0,    0  }
};

const char* const MSG_NO_GETMODE =
	"Problems: this device is not a LIRC kernel device (it does not\n"
	"support LIRC_GET_REC_MODE ioctl). This is not necessarily a\n"
	"problem, but mode2 will not work.  If you are using the --raw\n"
	"option you might try using without it and select a driver\n"
	"instead. Otherwise, try using lircd + irw to view the decoded\n"
	"data - this might very well work even if mode2 doesn't.";


static void add_defaults(void)
{
	char level[4];

	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());

	const char* const defaults[] = {
		"mode2:driver",	    "default",
		"mode2:lircdfile",  LIRCD,
		"lircd:logfile",    "syslog",
		"lircd:debug",	    level,
		"lircd:plugindir",  PLUGINDIR,
		"lircd:configfile", LIRCDCFGFILE,
		(const char*)NULL,  (const char*)NULL
	};
	options_add_defaults(defaults);
}


static void parse_options(int argc, char** argv)
{
	int c;
	static const char* const optstring = "hvd:H:mkrg:s:U:A:";
	char driver[64];

	strcpy(driver, "default");
	add_defaults();
	while ((c = getopt_long(argc, argv, optstring, options, NULL)) != -1) {
		switch (c) {
		case 'h':
			puts(help);
			exit(EXIT_SUCCESS);
		case 'H':
			strncpy(driver, optarg, sizeof(driver) - 1);
			break;
		case 'v':
			printf("%s %s\n", "mode2 ", VERSION);
			exit(EXIT_SUCCESS);
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case 'k':
			unsetenv("SUDO_USER");
			break;
		case 'd':
			opt_device = optarg;
			break;
		case 's':
			opt_dmode = 2;
			t_div = atoi(optarg);
			break;
		case 'm':
			opt_dmode = 1;
			break;
		case 'r':
			opt_raw_access = 1;
			break;
		case 'g':
			opt_gap = atoi(optarg);
			break;
		case 'A':
			options_set_opt("lircd:driver-options", optarg);
			break;
		default:
			printf("Usage: mode2 [options]\n");
			exit(EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fputs("Too many arguments\n", stderr);
		exit(EXIT_FAILURE);
	}
	if (opt_raw_access && opt_device == NULL) {
		fprintf(stderr, "The --raw option requires a --device\n");
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


/** Open device using curr_driver->open_func() and curr_driver->init_func().*/
int open_device(int opt_raw_access, const char* device)
{
	struct stat s;

	__u32 mode;
	int fd;
	const char* opt;

	if (opt_raw_access) {
		fd = open(device, O_RDONLY);
		if (fd == -1) {
			perror("Error while opening device");
			exit(EXIT_FAILURE);
		}
		;
		if ((fstat(fd, &s) != -1) && (S_ISFIFO(s.st_mode))) {
			/* can't do ioctls on a pipe */
		} else if ((fstat(fd, &s) != -1) && (!S_ISCHR(s.st_mode))) {
			fprintf(stderr, "%s is not a character device\n",
				device);
			fputs("Use the -d option to specify device\n",
			      stderr);
			close(fd);
			exit(EXIT_FAILURE);
		} else if (ioctl(fd, LIRC_GET_REC_MODE, &mode) == -1) {
			puts(MSG_NO_GETMODE);
			close(fd);
			exit(EXIT_FAILURE);
		}
	} else {
		curr_driver->open_func(device);
		opt = options_getstring("lircd:driver-options");
		if (opt != NULL)
			if (drv_handle_options(opt) != 0) {
				fprintf(stderr,
				"Cannot set driver (%s) options (%s)\n",
				curr_driver->name, opt);
				exit(EXIT_FAILURE);
			}
		if (!curr_driver->init_func || !curr_driver->init_func()) {
			fprintf(stderr, "Cannot initiate device %s\n",
				curr_driver->device);
			exit(EXIT_FAILURE);
		}
		printf("Using device: %s\n", curr_driver->device);
		fd = curr_driver->fd;   /* please compiler */
		mode = curr_driver->rec_mode;
		if (mode != LIRC_MODE_MODE2) {
			if (strcmp(curr_driver->name, "default") == 0) {
				puts("Please use the --raw option to access "
				     "the device directly instead through\n"
				     "the abstraction layer");
				exit(EXIT_FAILURE);
			} else if (mode != LIRC_MODE_LIRCCODE) {
				puts("Internal error: bad receive mode");
				exit(EXIT_FAILURE);
			}
		}
	}
	if (opt_device && strcmp(opt_device, LIRCD) == 0) {
		fputs("Refusing to connect to lircd socket\n", stderr);
		exit(EXIT_FAILURE);
	}
	printf("Using device: %s\n", curr_driver->device);
	return fd;
}


/** Define loglevel and open the log file. */
static void setup_log(void)
{
	char path[128];
	const char* loglevel;

	loglevel = getenv("LIRC_LOGLEVEL");
	if (loglevel == NULL) {
		loglevel = "LIRC_NOTICE";
	} else if (string2loglevel(loglevel) == LIRC_BADLEVEL) {
		fprintf(stderr, "Bad LIRC_LOGLEVEL: %s\n", loglevel);
		loglevel = "LIRC_NOTICE";
	}
	lirc_log_get_clientlog("mode2", path, sizeof(path));
	lirc_log_set_file(path);
	lirc_log_open("mode2", 1, string2loglevel(loglevel));
}


/** Get the codelength (bits per decoded value) for lirccode data. */
unsigned int get_codelength(int fd, int use_raw_access)
{
	unsigned int code_length;

	if (use_raw_access) {
		if (ioctl(fd, LIRC_GET_LENGTH, &code_length) == -1) {
			perror("Could not get code length");
			close(fd);
			exit(EXIT_FAILURE);
		}
	} else {
		code_length = curr_driver->code_length;
	}
	if (code_length > sizeof(ir_code) * CHAR_BIT) {
		fprintf(stderr, "Cannot handle %u bit codes\n",
			code_length);
		close(fd);
		exit(EXIT_FAILURE);
	}
	return code_length;
}


/** Print mode2 data as pulse/space durations, one per line. */
void print_mode2_data(unsigned int data)
{
	static int bitno = 1;

	switch (opt_dmode) {
	case 0:
		printf("%s %u\n", (
			       data & PULSE_BIT) ? "pulse" : "space",
		       (__u32)(data & PULSE_MASK));
		break;
	case 1: {
		/* print output like irrecord raw config file data */
		printf(" %8u", (__u32)data & PULSE_MASK);
		++bitno;
		if (data & PULSE_BIT) {
			if ((bitno & 1) == 0)
				/* not in expected order */
				fputs("-pulse", stdout);
		} else {
			if (bitno & 1)
				/* not in expected order */
				fputs("-space", stdout);
			if (((data & PULSE_MASK) > opt_gap) || (bitno >= 6)) {
				/* real long space or more
				 * than 6 codes, start new line */
				puts("");
				if ((data & PULSE_MASK) > opt_gap)
					puts("");
				bitno = 0;
			}
		}
		break;
	}
	case 2:
		if ((data & PULSE_MASK) > opt_gap) {
			fputs("_\n\n_", stdout);
		} else {
			printf("%.*s",
			       ((data & PULSE_MASK) + t_div / 2) / t_div,
			       (data & PULSE_BIT) ?
			       "------------" : "____________");
		}
		break;
	}
	fflush(stdout);
}


/** Print lirccode data as a decoded value (an integer) per line. */
void print_lirccode_data(char* buffer, size_t count)
{
	size_t i;

	fputs("code: 0x", stdout);
	for (i = 0; i < count; i++)
		printf("%02x", (unsigned char)buffer[i]);
	puts("");
	fflush(stdout);
}


int main(int argc, char** argv)
{
	int fd;
	char buffer[sizeof(ir_code)];
	lirc_t data;
	__u32 mode;
	size_t count = sizeof(lirc_t);
	int result;
	/**
	 * Was hard coded to 50000 but this is too long, the shortest gap in the
	 * supplied .conf files is 10826, the longest space defined for any one,
	 * zero or header is 7590
	 */
	__u32 code_length;

	hw_choose_driver(NULL);
	options_load(argc, argv, NULL, parse_options);
	setup_log();
	fd = open_device(opt_raw_access, opt_device);
	if (geteuid() == 0)
		drop_root_cli(setuid);
	mode = curr_driver->rec_mode;
	if (mode == LIRC_MODE_LIRCCODE) {
		code_length = get_codelength(fd, opt_raw_access);
		count = (code_length + CHAR_BIT - 1) / CHAR_BIT;
	}
	while (1) {
		if (opt_raw_access || mode != LIRC_MODE_MODE2) {
			result = read(fd,
				      (mode == LIRC_MODE_MODE2 ?
				       (void*)&data : buffer),
				      count);
			if (result != (int)count) {
				fputs("read() failed\n", stderr);
				break;
			}
			if (mode == LIRC_MODE_MODE2)
				print_mode2_data(data);
			else
				print_lirccode_data(buffer, count);
		} else {
			data = curr_driver->readdata(0);
			if (data == 0) {
				fputs("readdata() failed\n", stderr);
				break;
			}
			print_mode2_data(data);
		}
	}
	return EXIT_SUCCESS;
}

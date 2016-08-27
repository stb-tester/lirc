/****************************************************************************
** mode2.c *****************************************************************
****************************************************************************
*
* mode2 - shows the pulse/space length or decoded values of remote buttons.
*
*
* Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
* Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

/*
 * TODO: Close stuff (call curr_driver->close_func(), closing logs)
 * when a terminating signal arrives.
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
#include <glob.h>
#include <syslog.h>
#include <time.h>
#include <pwd.h>

#include "lirc_private.h"

static const logchannel_t logchannel = LOG_APP;

static const char* opt_device = NULL;
static const char* opt_driver = NULL;
static int opt_dmode = 0;
static int t_div = 500;
static unsigned int opt_gap = 10000;
static int opt_raw_access = 0;
static int opt_list_devices = 0;

static const char* const help =
	"Usage: mode2 [options]\n"
	"\t -d --device=device\tRead from given device\n"
	"\t -H --driver=driver\tUse given driver\n"
	"\t -U --plugindir=path\tLoad plugins from path\n"
	"\t -k --keep-root\t\tKeep root privileges\n"
	"\t -m --mode\t\tEnable column display mode\n"
	"\t -l --list-devices\tList available devices\n"
	"\t -r --raw\t\tAccess device directly without driver\n"
	"\t -g --gap=time\t\tTreat spaces longer than time as the gap\n"
	"\t -s --scope=time\t'Scope' like display with time us per char\n"
	"\t -A --driver-options=key:value[|key:value...]\n"
	"\t\t\t\tSet driver options\n"
	"\t -D --loglevel=level\t'error', 'info', 'notice',... or 3..10\n"
	"\t -O --options-file\tUse alternative lirc_options.conf file\n"
	"\t -h --help\t\tDisplay usage summary\n"
	"\t -v --version\t\tDisplay version\n";


static const struct option options[] = {
	{"help",           no_argument,       NULL, 'h'},
	{"debug",          required_argument, NULL, 'D'},
	{"version",        no_argument,       NULL, 'v'},
	{"device",         required_argument, NULL, 'd'},
	{"driver",         required_argument, NULL, 'H'},
	{"keep-root",      no_argument,       NULL, 'k'},
	{"list-devices",   no_argument,       NULL, 'l'},
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
"data - this might very well work even if mode2 doesn't.\n";

const char* const USE_RAW_MSG =
"Please use the --raw option to access the device directly instead"
" through the abstraction layer\n";

static void add_defaults(void)
{
	const char* const device = options_getstring("lircd:device");
	const char* const driver = options_getstring("lircd:driver");
	const char* const plugindir = options_getstring("lircd:plugindir");
	const char* const lircd_level = options_getstring("lircd:debug");
	const char* const env_level = getenv("LIRC_LOGELEVL");
	char level[12];

	if (env_level != NULL) {
		if (string2loglevel(env_level) == LIRC_BADLEVEL) {
			fprintf(stderr,
			        "Bad debug level: %s in LIRC_LOGLEVEL\n",
			        env_level);
			exit(EXIT_FAILURE);
		}
		strncpy(level, env_level, sizeof(level) - 1);
	} else if (lircd_level != NULL) {
		strncpy(level, lircd_level, sizeof(level) - 1);
	} else {
		snprintf(level, sizeof(level), " %d", lirc_log_defaultlevel());
	}
	const char* const defaults[] = {
		"mode2:driver",     driver ? driver : "default",
		"mode2:lircdfile",  LIRCD,
		"lircd:logfile",    "syslog",
		"mode2:plugindir",  plugindir ? plugindir : PLUGINDIR,
		"lircd:configfile", LIRCDCFGFILE,
		"mode2:debug",	    level,
		"mode2:device",     device ? device : LIRC_DRIVER_DEVICE,
		(const char*)NULL,  (const char*)NULL
	};
	options_add_defaults(defaults);
}


static void parse_options(int argc, char** argv)
{
	int c;
	static const char* const optstring = "hvD:d:H:mklrg:s:U:A:";

	add_defaults();
	while ((c = getopt_long(argc, argv, optstring, options, NULL)) != -1) {
		switch (c) {
		case 'D':
			if (string2loglevel(optarg) == LIRC_BADLEVEL) {
				fprintf(stderr, "Bad debug level: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			options_set_opt("mode2:debug", optarg);
			break;
		case 'h':
			puts(help);
			exit(EXIT_SUCCESS);
		case 'H':
			options_set_opt("mode2:driver", optarg);
			break;
		case 'v':
			printf("%s %s\n", "mode2 ", VERSION);
			exit(EXIT_SUCCESS);
		case 'U':
			options_set_opt("mode2:plugindir", optarg);
			break;
		case 'k':
			unsetenv("SUDO_USER");
			break;
		case 'l':
			opt_list_devices = 1;
			break;
		case 'd':
			options_set_opt("mode2:device", optarg);
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
			fprintf(stderr, "Usage: mode2 [options]\n");
			exit(EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		fputs("Too many arguments\n", stderr);
		exit(EXIT_FAILURE);
	}
	opt_device = options_getstring("mode2:device");
	if (opt_raw_access && opt_device == NULL) {
		fprintf(stderr, "The --raw option requires a --device\n");
		exit(EXIT_FAILURE);
	}
	options_set_opt("lircd:plugindir",
			options_getstring("mode2:plugindir"));
	opt_driver = options_getstring("mode2:driver");
	if (hw_choose_driver(opt_driver) != 0) {
		fprintf(stderr, "Driver `%s' not found.", opt_driver);
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

	log_info("Opening device: %s", device);
	if (opt_raw_access) {
		fd = open(device, O_RDONLY);
		if (fd == -1) {
			device = device ? device : "(Null)";
			perrorf("Error while opening device: %s", device);
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
			fputs(MSG_NO_GETMODE, stderr);
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
		fprintf(stderr, "Trying device: %s\n", curr_driver->device);
		fd = curr_driver->fd;   /* please compiler */
		mode = curr_driver->rec_mode;
		if (mode != LIRC_MODE_MODE2) {
			if (strcmp(curr_driver->name, "default") == 0) {
				fputs(USE_RAW_MSG, stderr);
				exit(EXIT_FAILURE);
			} else if (mode != LIRC_MODE_LIRCCODE) {
				fputs("Internal error: bad receive mode\n",
				      stderr);
				exit(EXIT_FAILURE);
			}
		}
	}
	if (opt_device && strcmp(opt_device, LIRCD) == 0) {
		fputs("Refusing to connect to lircd socket\n", stderr);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "Using device: %s\n", curr_driver->device);
	return fd;
}


/** Get the codelength (bits per decoded value) for lirccode data. */
unsigned int get_codelength(int fd, int use_raw_access)
{
	unsigned int code_length;
	int r = 1;

	if (use_raw_access) {
		if (ioctl(fd, LIRC_GET_LENGTH, &code_length) == -1) {
			perror("Could not get code length");
			close(fd);
			exit(EXIT_FAILURE);
		}
		return code_length;
	}
	if (curr_driver->drvctl_func) {
		r = curr_driver->drvctl_func(DRVCTL_GET_RAW_CODELENGTH,
					     &code_length);
	}
	if (r != 0)
		code_length = curr_driver->code_length;
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


/**
 * Process next button press and print a dump, return boolean OK/FAIL.
 */
int next_press(int fd, int mode, int bytes)
{
	char buffspace[bytes];
	int r;
        union {
		char* buffer;
		lirc_t data;
	} input;

	input.buffer = buffspace;
	if (opt_raw_access || mode != LIRC_MODE_MODE2) {
		r = read(fd, input.buffer, bytes);
		if (r == -1)
			perrorf("read() error on %s", opt_device);
		else if (r != (int)bytes)
			fprintf(stderr, "Partial read %d bytes on %s",
				r, opt_device);
		if (r != (int)bytes)
			return 0;
		if (mode == LIRC_MODE_MODE2)
			print_mode2_data(input.data);
		else
			print_lirccode_data(input.buffer, bytes);
	} else {
		input.data = curr_driver->readdata(0);
		if (input.data == 0) {
			fputs("readdata() failed\n", stderr);
			return 0;
		}
		print_mode2_data(input.data);
	}
	return 1;
}

static void list_devices(void)
{
	glob_t glob;
	unsigned i;

	if (!curr_driver->drvctl_func  ||
	    curr_driver->drvctl_func(DRVCTL_GET_DEVICES, &glob) != 0) {
		fputs("Device enumeration is not supported by this driver\n",
                      stderr);
		exit(1);
	}
	for (i = 0; i < glob.gl_pathc; i += 1)
		puts(glob.gl_pathv[i]);
	curr_driver->drvctl_func(DRVCTL_FREE_DEVICES, &glob);
}


int main(int argc, char** argv)
{
	int fd;
	__u32 mode;
	size_t bytes = sizeof(lirc_t);
	char logpath[256];
	/**
	 * Was hard coded to 50000 but this is too long, the shortest gap in the
	 * supplied .conf files is 10826, the longest space defined for any one,
	 * zero or header is 7590
	 */
	__u32 code_length;
	const loglevel_t level = options_get_app_loglevel("mode2");

	hw_choose_driver(NULL);
	options_load(argc, argv, NULL, parse_options);

	lirc_log_get_clientlog("mode2", logpath, sizeof(logpath));
	(void)unlink(logpath);
	lirc_log_set_file(logpath);
	lirc_log_open("mode2", 1, level);
	if (opt_list_devices) {
		list_devices();
		return 0;
	}
        if (opt_raw_access) {
		printf("Using raw access on device %s\n", opt_device);
	} else {
		printf("Using driver %s on device %s\n",
		       options_getstring("mode2:driver"),
		       opt_device);
	}
	fd = open_device(opt_raw_access, opt_device);
	if (geteuid() == 0)
		drop_root_cli(setuid);

	mode = curr_driver->rec_mode;
	if (mode == LIRC_MODE_LIRCCODE) {
		code_length = get_codelength(fd, opt_raw_access);
		bytes = (code_length + CHAR_BIT - 1) / CHAR_BIT;
	}
	while (next_press(fd, mode, bytes))
		;
	return EXIT_SUCCESS;
}

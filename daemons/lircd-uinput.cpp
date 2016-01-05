/****************************************************************************
** lircd-uinput.cpp ********************************************************
*****************************************************************************
*
* lircd-uinput - Forward decoded lircd events to kernel uinput device.
*
*/

/**
 * @file lircd-uinput.c
 * This file implements the uinput forwarding service.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <linux/input.h>
#include "lirc/input_map.h"

#include "lirc_private.h"

static const logchannel_t logchannel = LOG_APP;


static const char* const HELP =
	"Usage: lircd-uinput [options] [lircd output socket]\n"
	"\t -h --help\t\t\tDisplay this message\n"
	"\t -v --version\t\t\tDisplay version\n"
	"\t -O --options-file\t\tOptions file\n"
	"\t -u --uinput=uinput \t\tuinput device [/dev/uinput]\n"
	"\t -r --release-suffix=suffix \tRelease events suffix [_UP]\n"
        "\t -D[level] --loglevel[=level]\t'info', 'warning', 'notice', etc., or 3..10.\n"
	"\t -L --logfile=file\t\tLog file path (default: use syslog)'\n";


static const struct option lircd_options[] = {
	{ "help",	    no_argument,       NULL, 'h' },
	{ "version",	    no_argument,       NULL, 'v' },
	{ "options-file",   required_argument, NULL, 'O' },
	{ "uinput",	    required_argument, NULL, 'u' },
	{ "release-suffix", required_argument, NULL, 'r' },
	{ "loglevel",	    optional_argument, NULL, 'D' },
	{ "logfile",	    required_argument, NULL, 'L' },
	{ 0,		    0,		       0,    0	 }
};


static int uinputfd = -1;


static void lircd_add_defaults(void)
{
	char level[4];

	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());
        const char* suffix = options_getstring("lircd:release");
        suffix = suffix ? suffix : "_UP";

	const char* const defaults[] = {
		"lircd:debug",		level,
		"lircd-uinput:logfile",	"syslog",
		"lircd-uinput:uinput",	"/dev/uinput",
		"lircd:release",	suffix,
		(const char*)NULL,	(const char*)NULL
	};
	options_add_defaults(defaults);
}


static void lircd_uinput_parse_options(int argc, char** const argv)
{
	int c;
	const char* optstring = "hvO:u:D::L:";
	loglevel_t loglevel_opt;

	optind = 1;
	lircd_add_defaults();
	while ((c = getopt_long(argc, argv, optstring, lircd_options, NULL))
	       != -1) {
		switch (c) {
		case 'h':
			fputs(HELP, stdout);
			exit(EXIT_SUCCESS);
		case 'v':
			printf("lircd %s\n", VERSION);
			exit(EXIT_SUCCESS);
			break;
		case 'O':
			break;
		case 'u':
			options_set_opt("lircd-uinput:uinput", "True");
			break;
		case 'D':
			loglevel_opt = (loglevel_t) options_set_loglevel(
				optarg ? optarg : "debug");
			if (loglevel_opt == LIRC_BADLEVEL) {
				fprintf(stderr, "Bad level: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'L':
			options_set_opt("lircd-uinput:logfile", optarg);
			break;
		default:
			fputs("Usage: lirc.uinput [options] [config-file]\n",
                              stderr);
			exit(EXIT_FAILURE);
		}
	}
	if (optind == argc - 1) {
		options_set_opt("lircd:output", argv[optind]);
	} else if (optind != argc) {
		fputs("lircd-uinput: invalid argument count\n", stderr);
		exit(EXIT_FAILURE);
	}
}


int setup_uinputfd(const char* path)
{
	int fd;
	int key;
	struct uinput_user_dev dev;

	fd = open(path, O_RDWR);
	if (fd == -1) {
		log_perror_err("Cannot open uinput device: %s", path);
		return -1;
	}
	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, "lircd-uinput", sizeof(dev.name));
	dev.name[sizeof(dev.name) - 1] = 0;
	if (write(fd, &dev, sizeof(dev)) != sizeof(dev)
            || ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0
	    || ioctl(fd, UI_SET_EVBIT, EV_REP) != 0)
		goto setup_error;

	for (key = KEY_RESERVED; key <= KEY_UNKNOWN; key++)
		if (ioctl(fd, UI_SET_KEYBIT, key) != 0)
			goto setup_error;

	if (ioctl(fd, UI_DEV_CREATE) != 0)
		goto setup_error;
	return fd;

setup_error:
	log_perror_err("could not setup uinput");
	close(fd);
	return -1;
}


int ends_with(const char *str, const char *suffix)
{
	if (str == NULL && suffix == NULL)
		return 1;
	if (!str || !suffix)
		return 0;

	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


void send_message(const char* remote_name,
		  const char* button_name,
		  int reps,
		  const char* releasesuffix)
{
	linux_input_code input_code;
	struct input_event event;

	if (uinputfd == -1 )
		return;
	if (get_input_code(button_name, &input_code) == -1) {
		log_info("Dropping non-standard symbol %s ",
			 button_name == NULL ? "Null" : button_name);
		return;
	}

	memset(&event, 0, sizeof(event));
	event.type = EV_KEY;
	event.code = input_code;
        if (ends_with(button_name, releasesuffix))
		event.value = reps ? 2 : 1;
        else
		event.value = 0;
	if (write(uinputfd, &event, sizeof(event)) != sizeof(event)) {
		log_perror_err("Writing regular event to uinput failed");
	}
	/* Need to write sync event */
	memset(&event, 0, sizeof(event));
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	if (write(uinputfd, &event, sizeof(event)) != sizeof(event)) {
		log_perror_err("Writing EV_SYN to uinput failed");
	}
}


void lircd_uinput(const char* socket_path,
		  const char* device,
		  const char* releasesuffix)
{
        int fd;
	int r;

	char button[PACKET_SIZE + 1];
	char remote[PACKET_SIZE + 1];
        char buffer[PACKET_SIZE + 1];
	struct sockaddr_un addr;
	int reps;

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		log_perror_err("socket");
		exit(2);
	}
	// strcpy(addr.sun_path, "/home/mk/tmp/lirc/test/var/lircd.socket");
	if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		log_perror_err("Cannot connnect to socket %s", addr.sun_path);
		exit(2);
	}
        uinputfd = setup_uinputfd(device);
        if (uinputfd == -1) {
		log_error("Cannot setup uinput file descriptor.")
		exit(1);
	}
	while (1) {
		r = read(fd, buffer, 128);
		if (r == -1) {
			log_perror_err("read");
			exit(2);
		}
		if (r == 0)
			exit(0);
		r = sscanf(buffer, "%*x %x %s %s\n", &reps, button, remote);
		if (r == 3) {
			send_message(remote, button, reps, releasesuffix);
		} else {
			log_warn("Cannot parse line: %s", buffer);

		}
	}
}


int main(int argc, char** argv)
{
	const char* opt;
	const char* arg_socket = VARRUNDIR  "/lircd";
	const char* opt_device = "/dev/uinput";
	const char* opt_releasesuffix = "_UP";

	options_load(argc, argv, NULL, lircd_uinput_parse_options);
	opt = options_getstring("lircd:debug");
	if (options_set_loglevel(opt) == LIRC_BADLEVEL) {
		fprintf(stderr, "Bad configuration loglevel:%s\n", opt);
		fprintf(stderr, HELP);
		fprintf(stderr, "Falling back to 'info'\n");
	}
	opt = options_getstring("lircd:logfile");
	if (opt != NULL)
		lirc_log_set_file(opt);
	lirc_log_open("lircd", 1, LIRC_INFO);
        arg_socket = options_getstring("lircd:output");
        opt_device = options_getstring("lircd-uinput:uinput");
        opt_releasesuffix = options_getstring("lircd:release");
	lircd_uinput(arg_socket, opt_device, opt_releasesuffix);
}

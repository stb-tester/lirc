/****************************************************************************
** irexec.c ****************************************************************
****************************************************************************
*
* irexec  - execute programs according to the pressed remote control buttons
*
* Copyright (C) 1998 Trent Piepho <xyzzy@u.washington.edu>
* Copyright (C) 1998 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "lirc_client.h"
#include "lirc_log.h"

static const char* const USAGE =
	"Usage: irexec [options] [lircrc config_file]\n"
	"\t-h --help\t\tDisplay usage summary\n"
	"\t-v --version\t\tDisplay version\n"
	"\t-d --daemon\t\tRun in background\n"
	"\t-D --loglevel=level\t'error', 'info', 'notice',... or 0..10\n"
	"\t-n --name\t\tUse this program name\n";

static const struct option options[] = {
	{ "help",     no_argument,	 NULL, 'h' },
	{ "version",  no_argument,	 NULL, 'v' },
	{ "daemon",   no_argument,	 NULL, 'd' },
	{ "name",     required_argument, NULL, 'n' },
	{ "loglevel", required_argument, NULL, 'D' },
	{ 0,	      0,		 0,    0   }
};

static int opt_daemonize = 0;
static loglevel_t opt_loglevel = LIRC_NOLOG;
static const char* opt_progname = "irexec";

static char path[256] = {0};


static void run_command(const char* cmd)
{
	int r;

	if (!opt_daemonize)
		logprintf(LIRC_DEBUG,
			  "Execing command \"%s\"", cmd);
	r = system(cmd);
	if (r != 0)
		logprintf(LIRC_NOTICE,
			  "Shell returned %d for \"%c\"", r, cmd);
}


static void irexec(struct lirc_config* config)
{
	char* code;
	char* c;
	int ret;


	while (lirc_nextcode(&code) == 0) {
		if (code == NULL)
			continue;
		while ((ret = lirc_code2char(config, code, &c)) == 0
		       && c != NULL)
			run_command(c);
		free(code);
		if (ret == -1)
			break;
	}
}


int main(int argc, char* argv[])
{
	struct lirc_config* config;
	const char* cf;
	int c;

	while ((c = getopt_long(argc, argv, "D:hvdn:", options, NULL)) != -1) {
		switch (c) {
		case 'h':
			puts(USAGE);
			return EXIT_SUCCESS;
		case 'v':
			puts("irexec " VERSION);
			return EXIT_SUCCESS;
		case 'd':
			opt_daemonize = 1;
			break;
		case 'n':
			opt_progname = optarg;
			break;
		case 'D':
			opt_loglevel = string2loglevel(optarg);
			if (opt_loglevel == LIRC_BADLEVEL) {
				fprintf(stderr, "Bad debug level: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			break;
		default:
			fprintf(stderr, "Bad option: %c\n", c);
			fputs(USAGE, stderr);
			return EXIT_FAILURE;
		}
	}
	if (optind < argc - 1) {
		fputs("Too many arguments\n", stderr);
		return EXIT_FAILURE;
	}
	if (opt_daemonize) {
		if (daemon(0, 0) == -1) {
			perror("Can't daemonize");
			lirc_freeconfig(config);
			lirc_deinit();
			exit(EXIT_FAILURE);
		}
	}

	cf = optind != argc ? argv[optind] : NULL;
	if (lirc_readconfig(cf, &config, NULL) != 0) {
		fputs("Cannot parse config file\n", stderr);
		exit(EXIT_FAILURE);
	}
	lirc_log_get_clientlog("irexec", path, sizeof(path));
	unlink(path);
	lirc_log_set_file(path);
	lirc_log_open("irexec", 1, opt_loglevel);

	if (lirc_init(opt_progname, opt_daemonize ? 0 : 1) == -1)
		exit(EXIT_FAILURE);

	irexec(config);
	lirc_freeconfig(config);
	lirc_deinit();
	exit(EXIT_SUCCESS);
}

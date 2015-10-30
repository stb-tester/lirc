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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
#include <sys/types.h>
#include <sys/wait.h>

#include "lirc_client.h"
#include "lirc_log.h"

static const logchannel_t logchannel = LOG_APP;

static const char* const USAGE =
	"Usage: irexec [options] [lircrc config_file]\n"
	"\t-d --daemon\t\tRun in background\n"
	"\t-D --loglevel=level\t'error', 'info', 'notice',... or 0..10\n"
	"\t-n --name=progname\tUse this program name for lircrc matching\n"
	"\t-h --help\t\tDisplay usage summary\n"
	"\t-v --version\t\tDisplay version\n";

static const struct option options[] = {
	{ "help",     no_argument,	 NULL, 'h' },
	{ "version",  no_argument,	 NULL, 'v' },
	{ "daemon",   no_argument,	 NULL, 'd' },
	{ "name",     required_argument, NULL, 'n' },
	{ "loglevel", required_argument, NULL, 'D' },
	{ 0,          0,		 0,    0   }
};

static int opt_daemonize	= 0;
static loglevel_t opt_loglevel	= LIRC_NOLOG;
static const char* opt_progname	= "irexec";

static char path[256] = {0};


/** Run shell command line in isolated process using double fork(). */
static void run_command(const char* cmd)
{
	pid_t pid1;
	pid_t pid2;

	pid1 = fork();
	if (pid1 < 0) {
		log_perror_err("Cannot fork");
		perror("Cannot fork()");
		exit(EXIT_FAILURE);
	}
	if (pid1 == 0) {
		pid2 = fork();
		if (pid2 < 0) {
			log_perror_err("Cannot do secondary fork()");
			exit(EXIT_FAILURE);
		}
		if (pid2 == 0) {
			log_debug("Execing command \"%s\"", cmd);
			char* const vp[] = {strdup(SH_PATH),
			      strdup("-c"),
			      strdup(cmd),
			      NULL
			};
			execvp(SH_PATH, vp);
			/* not reached */
			log_perror_err("execvp failed");
			fputs("execvp failed\n", stderr);
		} else {
			waitpid(pid2, NULL, WNOHANG);
			exit(0);
		}
	} else {
		waitpid(pid1, NULL, 0);
	}
}


/** Get buttonclick messages from lircd socket and process them. */
static void process_input(struct lirc_config* config)
{
	char* code;
	char* c;
	int r;

	while (lirc_nextcode(&code) == 0) {
		if (code == NULL)
			continue;
		r = lirc_code2char(config, code, &c);
		while (r == 0 && c != NULL) {
			run_command(c);
			r = lirc_code2char(config, code, &c);
		}
		free(code);
		if (r == -1)
			break;
	}
}


int irexec(const char* configfile)
{
	struct lirc_config* config;

	if (opt_daemonize) {
		if (daemon(0, 0) == -1) {
			perror("Can't daemonize");
			return EXIT_FAILURE;
		}
	}
	if (lirc_init(opt_progname, opt_daemonize ? 0 : 1) == -1)
		return EXIT_FAILURE;

	if (lirc_readconfig(configfile, &config, NULL) != 0) {
		fputs("Cannot parse config file\n", stderr);
		return EXIT_FAILURE;
	}
	lirc_log_get_clientlog("irexec", path, sizeof(path));
	unlink(path);
	lirc_log_set_file(path);
	lirc_log_open("irexec", 1, opt_loglevel);

	process_input(config);
	lirc_deinit();

	lirc_freeconfig(config);
	return EXIT_SUCCESS;
}


int main(int argc, char* argv[])
{
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
			break;
		default:
			fputs(USAGE, stderr);
			return EXIT_FAILURE;
		}
	}
	if (optind < argc - 1) {
		fputs("Too many arguments\n", stderr);
		return EXIT_FAILURE;
	}
	if (opt_loglevel == LIRC_BADLEVEL) {
		fprintf(stderr, "Bad debug level: %s\n", optarg);
		return EXIT_FAILURE;
	}
	return irexec(optind != argc ? argv[optind] : NULL);
}

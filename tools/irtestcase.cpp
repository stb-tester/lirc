/****************************************************************************
** irtestcase.c ************************************************************
****************************************************************************
*
* irtestcase - Log synced streams of raw durations, codes and app strings.
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <getopt.h>

#include "lirc_client.h"
#include "lirc_private.h"

static const char* const USAGE =
	"Synopsis:\n"
	"irtestcase [-p prog -l lircrc] [-t testdata] <socket>\n"
	"irtestcase [ħ | -v]\n\n"
	"<socket> is the socket connecting to lircd. Defaults to a hardcoded\n"
	"default value, usually /var/run/lirc/lircd. Respects LIRC_SOCKET_PATH in\n"
	"environment.\n\n"
	"Options:\n"
	"   -l  lircrc  Log also translated symbols using lircrc type config file.\n"
	"   -p  prog    Program name used to match entries in lircrc.\n"
	"   -t  path    Use testdata from path.\n"
	"   -v  version Print version.\n"
	"   -h  help    Print this message.\n";

static struct option opts[] = {
	{ "prog",     required_argument, NULL, 'p' },
	{ "lircrc",   required_argument, NULL, 'l' },
	{ "testdata", required_argument, NULL, 't' },
	{ "help",     no_argument,	 NULL, 'h' },
	{ "version",  no_argument,	 NULL, 'v' },
	{ 0,	      0,		 0,    0   }
};

#define LOGDIR          "/tmp/irtestcase"
#define DEVICE_LOG      LOGDIR  "/durations.log"
#define CODE_LOG        LOGDIR  "/codes.log"
#define APP_LOG         LOGDIR  "/app_strings.log"

#define DEFAULT_PROG    "no_such_prog"

/** Delay (us) before sending testdata so we are listening when it comes.*/
static const int TESTDATA_DELAY = 1000000;

static const char* opt_testdata = NULL;
static const char* opt_lircrc = NULL;
static const char* opt_prog = DEFAULT_PROG;

static FILE* app_log = NULL;
static FILE* code_log = NULL;


/** Configure lircd to log received data from driver in path. */
static void set_devicelog(int fd, const char* path)
{
	int r;
	lirc_cmd_ctx command;

	unlink(path);
	lirc_command_init(&command, "SET_INPUTLOG %s\n", path);
	r = lirc_command_run(&command, fd);
	if (r != 0) {
		fprintf(stderr, "Cannot set lircd device log %s\n", path);
		exit(2);
	}
}


/** Configure lircd (file driver) to read testdata from path. */
static void set_testinput(int fd, const char* path)
{
	int r;
	lirc_cmd_ctx command;

	lirc_command_init(&command, "DRV_OPTION set-infile %s\n", path);
	r = lirc_command_run(&command, fd);
	if (r != 0) {
		fputs("Cannot set test input file\n", stderr);
		exit(2);
	}
}


/** Setup and clear the hardcoded output directory. */
static void init_testdir(void)
{
	unlink(CODE_LOG);
	unlink(APP_LOG);
	unlink(DEVICE_LOG);

	mkdir(LOGDIR, 0755);
	if (access(LOGDIR, F_OK) != 0) {
		fprintf(stderr, "Cannot create log directory: %s\n", LOGDIR);
		exit(2);
	}
	code_log = fopen(CODE_LOG, "w");
	if (code_log == NULL) {
		fprintf(stderr, "Cannot open %s, giving up\n", CODE_LOG);
		exit(2);
	}
	if (opt_lircrc != NULL) {
		app_log = fopen(APP_LOG, "w");
		if (app_log == NULL) {
			fprintf(stderr,
				"Cannot open %s, giving up\n", APP_LOG);
			exit(2);
		}
	}
}


/** Get next code from lircd. */
static int nextcode(int fd, char* buff, ssize_t size)
{
	int i;

	i = read(fd, buff, size);
	if (i == -1) {
		perror("read");
		exit(errno);
	}
	;
	if (strstr(buff, "__EOF") != NULL) {
		puts("Exit on EOF");
		exit(0);
	}
	if (i >= 0)
		buff[i] = '\0';
	return i > 0 ? 1 : 0;
}


/**  Send testdata after delay. This is fork(), so nothing comes back. */
static void send_later(int fd, const char* path)
{
	int r;

	r = fork();
	if (r == 0) {
		usleep(TESTDATA_DELAY);
		set_testinput(fd, path);
		exit(0);
	} else if (r < 0) {
		perror("Cannot fork(!)");
		exit(3);
	} else {
		//parent, just proceed.
	}
}


/** Run testcase. */
static int irtestcase(int fd_io, int fd_cmd)
{
	int r;
	struct lirc_config* config;
	char code[64];
	char* c;

	if (opt_lircrc != NULL) {
		if (lirc_readconfig_only(opt_lircrc, &config, NULL) != 0) {
			fputs("Cannot initiate lircrc decoding\n", stderr);
			exit(2);
		}
	}
	while (nextcode(fd_io, code, sizeof(code)) == 1) {
		fputs(code, stdout);
		if (strstr(code, "__EOF") != NULL)
			exit(0);
		fputs(code, code_log);
		if (opt_lircrc != NULL) {
			r = lirc_code2char(config, code, &c);
			while (r == 0 && c != NULL) {
				printf("    %s\n", c);
				fprintf(app_log, "%s\n", c);
				r = lirc_code2char(config, code, &c);
			}
			fflush(app_log);
		}
		fflush(stdout);
		fflush(code_log);
	}
	return 0;
}


int main(int argc, char* argv[])
{
	int fd_io;
	int fd_cmd;
	const char* socketpath;
	char path[128];
	int c;
	const loglevel_t level = options_get_app_loglevel("irtestcase");

	while ((c = getopt_long(argc, argv, "hl:p:t:v", opts, NULL)) != EOF) {
		switch (c) {
		case 'l':
			opt_lircrc = optarg;
			break;
		case 'p':
			opt_prog = optarg;
			break;
		case 't':
			opt_testdata = optarg;
			break;
		case 'h':
			puts(USAGE);
			return EXIT_SUCCESS;
		case 'v':
			printf("%s\n", "irtestcase " VERSION);
			return EXIT_SUCCESS;
		case '?':
			fprintf(stderr, "unrecognized option: -%c\n", optopt);
			fputs("Try `irtestcase --help'.\n", stderr);
			return EXIT_FAILURE;
		}
	}
	if (argc > optind + 1) {
		fputs("irtestcase: Too many arguments (max one).\n", stderr);
		fputs("Try `irtestcase --help'.\n", stderr);
		return EXIT_FAILURE;
	}
	if (strcmp(opt_prog, DEFAULT_PROG) != 0 && opt_lircrc == NULL) {
		fputs("--prog requires --lircrc/-l. Giving up.\n", stderr);
		return EXIT_FAILURE;
	}
	if (opt_lircrc != NULL && strcmp(opt_prog, DEFAULT_PROG) == 0) {
		fputs("--lircrc requires --prog/-p. Giving up.\n", stderr);
		return EXIT_FAILURE;
	}

	init_testdir();
	socketpath = argc == optind + 1 ? argv[optind] : NULL;
	socketpath = socketpath ? socketpath : getenv("LIRC_SOCKET_PATH");
	socketpath = socketpath ? socketpath : LIRCD;
	fd_cmd = lirc_get_local_socket(socketpath, 1);
	if (fd_cmd < 0) {
		fputs("Cannot open lircd socket.\n", stderr);
		exit(3);
	}
	set_devicelog(fd_cmd, DEVICE_LOG);
	if (opt_testdata != NULL)
		send_later(fd_cmd, opt_testdata);

	lirc_log_get_clientlog("irtestcase", path, sizeof(path));
	lirc_log_set_file(path);
	lirc_log_open("irtestcase", 1, level);

	setenv("LIRC_SOCKET_PATH", socketpath, 1);
	fd_io = lirc_init(opt_prog, 1);
	if (fd_io < 0) {
		fputs("Cannot run lirc_init.\n", stderr);
		exit(3);
	}

	return irtestcase(fd_io, fd_cmd);
}

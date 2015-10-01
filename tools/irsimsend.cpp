/****************************************************************************
** irsimsend.c *************************************************************
****************************************************************************
*
* irsimsend - send all codes defined in a given config file.
*
*/

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

#include "lirc_private.h"
#include "lirc_client.h"

static const logchannel_t logchannel = LOG_APP;

static const char* const USAGE =
	"Send key symbols durations to fixed file 'simsend.out'.\n\n"
	"Synopsis:\n"
	"    irsimsend [-U path] [-s time] [-c count] <file>\n"
	"    irsimsend [-k keysym | -l listfile] [-U path] [-s time] [-c count]"
	" <file>\n"
	"    irsimsend [-h | -v]\n\n"
	"<file> is a lircd.conf type config file. In the first form, all keys in\n"
	"that file are sent.\n\n"
	"Options:\n"
	"    -U, --plugindir <path>:     Load drivers from <path>.\n"
	"    -c, --config <count>:       Repeat each key <count> times.\n"
	"    -l, --listfile <file>       Send symbols in file.\n"
	"    -k, --keysym <keysym>       Send a single keysym.\n"
	"    -s, --start-space <time>    Send a start space <time> us\n"
	"    -v, --version               Print version.\n"
	"    -h, --help                  Print this message.\n";

static struct option options[] = {
	{ "help",	 no_argument,	    NULL, 'h' },
	{ "version",	 no_argument,	    NULL, 'v' },
	{ "count",	 required_argument, NULL, 'c' },
	{ "keysym",	 required_argument, NULL, 'k' },
	{ "listfile",	 required_argument, NULL, 'l' },
	{ "pluginpath",	 required_argument, NULL, 'U' },
	{ "start-space", required_argument, NULL, 's' },
	{ 0,		 0,		    0,	  0   }
};

static const char* const OUTFILE = "simsend.out";

static int opt_count = 1;
static int opt_startspace = -1;
static const char* opt_keysym = NULL;
static const char* opt_listfile = NULL;


/** Set up default values for all command line options + filename. */
static void add_defaults(void)
{
	static char plugindir[128];
	const char* defaults[] = {
		"lircd:plugindir",	      plugindir,
		"irsimsend:driver",	      "devinput",
		"irsimsend:count",	      "1",
		"irsimsend:keysym",	      (const char*)NULL,
		"irsimsend:listfile",	      (const char*)NULL,
		"irsimsend:start-space"	      "-1",
		(const char*)NULL,	      (const char*)NULL
	};
	const char* s = getenv("LIRC_PLUGIN_PATH");

	strncpy(plugindir, s != NULL ? s : PLUGINDIR, sizeof(plugindir) - 1);
	options_add_defaults(defaults);
};


int parse_uint_arg(const char* optind, const char* errmsg)
{
	long c;

	c = strtol(optarg, NULL, 10);
	if (c > INT_MAX || c < 0 || errno == EINVAL || errno == ERANGE) {
		fputs(errmsg, stderr);
		exit(EXIT_FAILURE);
	}
	return (int)c;
}


static void parse_options(int argc, char** const argv)
{
	long c;

	add_defaults();

	while ((c = getopt_long(argc, argv, "c:hk:l:s:U:v", options, NULL))
	       != EOF) {
		switch (c) {
		case 'h':
			puts(USAGE);
			exit(EXIT_SUCCESS);
		case 'c':
			(void) parse_uint_arg(optarg, "Illegal count value\n");
			options_set_opt("irsimsend:count", optarg);
			break;
		case 'v':
			printf("%s\n", "irw " VERSION);
			exit(EXIT_SUCCESS);
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case 'k':
			options_set_opt("irsimsend:keysym", optarg);
			break;
		case 'l':
			options_set_opt("irsimsend:listfile", optarg);
			break;
		case 's':
			(void) parse_uint_arg(optarg, "Illegal space value\n");
			options_set_opt("irsimsend:start-space", optarg);
			break;
		case '?':
			fprintf(stderr, "unrecognized option: -%c\n", optopt);
			fputs("Try `irsimsend -h' for more information.\n",
			      stderr);
			exit(EXIT_FAILURE);
		}
	}
	if (argc != optind + 1) {
		fputs(USAGE, stderr);
		exit(EXIT_FAILURE);
	}
}


static struct ir_remote* setup(const char* path)
{
	struct ir_remote* remote;
	FILE* f;

	if (hw_choose_driver("file") == -1) {
		fputs("Cannot load file driver (bad plugin path?)\n",
		      stderr);
		exit(EXIT_FAILURE);
	}
	unlink(OUTFILE);
	curr_driver->open_func(OUTFILE);
	curr_driver->init_func();

	if (opt_listfile != NULL && access(opt_listfile, R_OK) != 0) {
		fprintf(stderr, "Cannot open %s for read\n", opt_listfile);
		exit(EXIT_FAILURE);
	}
	f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open %s for read\n", path);
		exit(EXIT_FAILURE);
	}
	remote = read_config(f, path);
	fclose(f);
	if (remote == NULL) {
		fprintf(stderr, "Cannot parse %s\n", path);
		exit(EXIT_FAILURE);
	}
	remote->min_repeat = 0;
	return remote;
}


static void send_space(int duration)
{
	char buff[64];

	snprintf(buff, sizeof(buff), "send-space:%d", duration);
	drv_handle_options(buff);
}


static void send_code(struct ir_remote* remote, struct ir_ncode* code)
{
	int i;
	static char last_code[32] = { '\0' };

	code->transmit_state = NULL;
	if (has_toggle_mask(remote))
		remote->toggle_mask_state = 0;
	if (has_toggle_bit_mask(remote))
		remote->toggle_bit_mask_state =
			(remote->toggle_bit_mask_state ^ remote->toggle_bit_mask);
	code->transmit_state = NULL;
	remote->min_repeat = 0;
	if (strcmp(code->name, last_code) == 0)
		repeat_remote = remote;
	send_ir_ncode(remote, code, 0);
	repeat_remote = remote;
	for (i = 1; i < opt_count; i += 1)
		send_ir_ncode(remote, code, 0);
	repeat_remote = NULL;
	strncpy(last_code, code->name, sizeof(last_code));
}


static int simsend_remote(struct ir_remote* remote)
{
	struct ir_ncode* code;

	while (remote != NULL) {
		for (code = remote->codes; code->name != NULL; code++) {
			printf("%s\n", code->name);
			send_code(remote, code);
		}
		remote = remote->next;
	}
	return 0;
}


static int simsend_keysym(struct ir_remote* remote, const char* keysym)
{
	struct ir_ncode* code;

	code = get_code_by_name(remote, keysym);
	if (code != NULL) {
		send_code(remote, code);
		printf("%s\n", keysym);
	} else {
		fprintf(stderr, "No such key: %s\n", keysym);
		exit(EXIT_FAILURE);
	}
	return 0;
}


static int simsend_list(struct ir_remote* remote)
{
	char line[256];
	char keysym[32];
	struct ir_ncode* code;
	FILE* f;
	char* s;
	int r;

	f = fopen(opt_listfile, "r");
	s = fgets(line, sizeof(line), f);
	while (s != NULL) {
		r = sscanf(line, "%*x %*x %32s %*s", keysym);
		if (r != 1)
			r = sscanf(line, "%32s", keysym);
		if (r != 1) {
			printf("Cannot parse line: %s\n", line);
			continue;
		}
		code = get_code_by_name(remote, keysym);
		if (code == NULL) {
			printf("Illegal keycode: %s\n", keysym);
		} else {
			printf("%s\n", code->name);
			send_code(remote, code);
		}
		s = fgets(line, sizeof(line), f);
	}
	fclose(f);
	return 0;
}

int main(int argc, char* argv[])
{
	struct ir_remote* remote;
	char path[128];
	const loglevel_t level = options_get_app_loglevel("irsimsimsend");

	lirc_log_get_clientlog("irsimsend", path, sizeof(path));
	lirc_log_set_file(path);
	lirc_log_open("irsimsend", 1, level);

	options_load(argc, argv, NULL, parse_options);
        opt_startspace = options_getint("irsimsend:start-space");
        opt_count = options_getint("irsimsend:count");
        opt_keysym = options_getstring("irsimsend:keysym");
        opt_listfile = options_getstring("irsimsend:listfile");

	remote = setup(argv[optind]);
	if (opt_startspace != -1)
		send_space(opt_startspace);
	if (opt_keysym != NULL)
		simsend_keysym(remote, opt_keysym);
	else if (opt_listfile != NULL)
		simsend_list(remote);
	else
		simsend_remote(remote);
	return 0;
}

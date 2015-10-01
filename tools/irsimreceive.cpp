/****************************************************************************
** irsimreceive.c **********************************************************
****************************************************************************
*
* irsimreceive.c -Receive data from file and decode.
*
*/

#include <config.h>

#include <stdlib.h>
#include <getopt.h>

#include "lirc_private.h"
#include "lirc_client.h"

static const logchannel_t logchannel = LOG_APP;

static void add_defaults(void)
{
	static char plugindir[128];
        const static char* defaults[] = {
		"lircd:plugindir",	plugindir,
		(const char*)NULL,	(const char*)NULL
	};
	const char* s = getenv("LIRC_PLUGIN_PATH");

	strncpy(plugindir, s != NULL ? s : PLUGINDIR, sizeof(plugindir) - 1);
	options_add_defaults(defaults);
}


static const char* const USAGE =
	"Usage: irsimreceive [options]  <configfile>  <datafile>\n\n"
	"<configfile> is a lircd.conf type configuration.\n"
	"<datafile> is a list of pulse/space durations.\n\n"
	"Options:\n"
	"    -U, --plugindir <path>:     Load drivers from <path>.\n"
	"    -v, --version               Print version.\n"
	"    -h, --help                  Print this message.\n";

static struct option options[] = {
	{ "help",	no_argument,	   NULL, 'h' },
	{ "version",	no_argument,	   NULL, 'v' },
	{ "pluginpath", required_argument, NULL, 'U' },
	{ 0,		0,		   0,	 0   }
};


static void parse_options(int argc, char** const argv)
{
	long c;

	add_defaults();

	while ((c = getopt_long(argc, argv, "hvU:", options, NULL))
	       != EOF) {
		switch (c) {
		case 'h':
			fputs(USAGE, stdout);
			exit(EXIT_SUCCESS);
		case 'v':
			printf("%s\n", "irw " VERSION);
			exit(EXIT_SUCCESS);
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case '?':
			fprintf(stderr, "unrecognized option: -%c\n", optopt);
			fputs("Try `irsimsend -h' for more information.\n",
			      stderr);
			exit(EXIT_FAILURE);
		}
	}
	if (argc != optind + 2) {
		fputs(USAGE, stderr);
		exit(EXIT_FAILURE);
	}
}


static void setup(const char* path)
{
	struct option_t option;
	int r;

	if (access(path, R_OK) != 0) {
		fprintf(stderr, "Cannot open %s for read\n", path);
		exit(EXIT_FAILURE);
	}
	if (hw_choose_driver("file") == -1) {
		fputs("Cannot load file driver (bad plugin path?)\n",
		      stderr);
		exit(EXIT_FAILURE);
	}
	r = curr_driver->open_func("dummy.out");
	if (r == 0) {
		fputs("Cannot open driver\n", stderr);
		exit(EXIT_FAILURE);
	}
	r = curr_driver->init_func();
	if (r == 0) {
		fputs("Cannot init driver\n", stderr);
		exit(EXIT_FAILURE);
	}
	strcpy(option.key, "set-infile");
	strncpy(option.value, path, sizeof(option.value));
	r = curr_driver->drvctl_func(DRVCTL_SET_OPTION, (void*)&option);
	if (r != 0) {
		fputs("Cannot set driver infile.\n", stderr);
		exit(EXIT_FAILURE);
	}
}


struct ir_remote* read_lircd_conf(const char* configfile)
{
	FILE* f;

	struct ir_remote* remotes;
	const char* filename = configfile;

	filename = configfile == NULL ? LIRCDCFGFILE : configfile;
	f = fopen(filename, "r");
	if (f == NULL) {
		log_perror_err("could not open config file '%s'", filename);
		exit(EXIT_FAILURE);
	}
	remotes = read_config(f, configfile);
	fclose(f);
	if (remotes == (void*)-1) {
		log_error("reading of config file failed");
		exit(EXIT_FAILURE);
	} else {
		log_debug("config file read");
		if (remotes == NULL) {
			log_error("config file contains no valid remote control definition");
			exit(EXIT_FAILURE);
		}
	}
	return remotes;
}


void printcode(char* s)
{
	int len;

	if (s == NULL) {
		puts("None");
	} else {
		len = strlen(s);
		if (strlen(s) > 2 && s[len - 1] == '\n')
			s[len - 1] = '\0';
		printf("%s\n", s);
	}
}


int simreceive(struct ir_remote* remotes)
{
	char* code = NULL;
	int at_eof;

	do {
		code = curr_driver->rec_func(remotes);
		at_eof = code != NULL && strstr(code, "__EOF") != NULL;
		if (code != NULL && !at_eof) {
			printcode(code);
			fflush(stdout);
		}
	} while (!at_eof);
	return 0;
}


int main(int argc, char* argv[])
{
	struct ir_remote* remotes;
	char path[128];
	const loglevel_t level = options_get_app_loglevel("irsimreceive");

	lirc_log_get_clientlog("irsimreceive", path, sizeof(path));
	lirc_log_set_file(path);
	lirc_log_open("irsimreceive", 1, level);

	options_load(argc, argv, NULL, parse_options);
	setup(argv[optind + 1]);
	remotes = read_lircd_conf(argv[optind]);
	return simreceive(remotes);
}

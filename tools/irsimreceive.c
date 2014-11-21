
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


static const char* const USAGE =
	"Usage: irsimreceive [options]  <configfile>  <datafile>\n\n"
	"<configfile> is a lircd.conf type configuration.\n"
	"<datafile> is a list of pulse/space durations.\n\n"
        "Options:\n"
        "    -U, --plugindir <path>:     Load drivers from <path>.\n"
        "    -v, --version               Print version.\n"
	"    -h, --help                  Print this message.\n";

static struct option options[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{"pluginpath", required_argument, NULL, 'U'},
	{0, 0, 0, 0}
};


static void setup(const char* path)
{
	struct option_t option;
	int r;

	if (access(path, R_OK) != 0) {
		fprintf(stderr, "Cannot open %s for read\n", path);
		exit(EXIT_FAILURE);
	}
	if (hw_choose_driver("file") == -1) {
		fprintf(stderr,
			"Cannot load file driver (bad plugin path?)\n");
		exit(EXIT_FAILURE);
	}
	r = curr_driver->open_func("dummy.out");
	if (r == 0) {
		fprintf(stderr, "Cannot open driver\n");
		exit(EXIT_FAILURE);
	}
	r = curr_driver->init_func();
	if (r == 0) {
		fprintf(stderr, "Cannot init driver\n");
		exit(EXIT_FAILURE);
	}
	strcpy(option.key, "set-infile");
	strncpy(option.value, path, sizeof(option.value));
	r = curr_driver->drvctl_func(DRVCTL_SET_OPTION, (void*) &option);
	if (r != 0) {
		fprintf(stderr, "Cannot set driver infile.\n");
		exit(EXIT_FAILURE);
	}
}


struct ir_remote* read_lircd_conf(const char* configfile)
{
	FILE *f;

	struct ir_remote* remotes;
	const char *filename = configfile;

	filename = configfile == NULL ? LIRCDCFGFILE : configfile;
	f = fopen(filename, "r");
	if (f == NULL) {
		logprintf(LIRC_ERROR, "could not open config file '%s'", filename);
		logperror(LIRC_ERROR, NULL);
		exit(EXIT_FAILURE);
	}
	remotes = read_config(f, configfile);
	fclose(f);
	if (remotes == (void *)-1) {
		logprintf(LIRC_ERROR, "reading of config file failed");
		exit(EXIT_FAILURE);
	} else {
		logprintf(LIRC_DEBUG, "config file read");
		if (remotes == NULL) {
			logprintf(LIRC_ERROR, "config file contains no valid remote control definition");
			exit(EXIT_FAILURE);
		}
	}
	return remotes;
}


void printcode(char* s)
{
	int len;

	if (s == NULL) {
		printf( "None\n");
	} else {
		len = strlen(s);
                if (strlen(s) > 2 && s[len -1] == '\n') {
			s[len - 1] = '\0';
		}
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


int main(int argc, char *argv[])
{
	long c;
	struct ir_remote* remotes;
	char path[128];

	while ((c = getopt_long(argc, argv, "hvc:U:", options, NULL))
	       != EOF) {
		switch (c) {
		case 'h':
			printf(USAGE);
			return (EXIT_SUCCESS);
		case 'v':
			printf("%s\n", "irw " VERSION);
			return (EXIT_SUCCESS);
		case 'U':
			options_set_opt("lircd:pluginpath", optarg);
			break;
		case '?':
			fprintf(stderr, "unrecognized option: -%c\n", optopt);
			fprintf(stderr,
                                "Try `irsimsend -h' for more information.\n");
			return (EXIT_FAILURE);
		}
	}
	if (argc != optind + 2) {
		fprintf(stderr, USAGE);
		return EXIT_FAILURE;
	}
	lirc_log_get_clientlog("irsimreceive", path, sizeof(path));
	lirc_log_set_file(path);
	lirc_log_open("irsimreceive", 1, LIRC_ERROR);
        setup(argv[optind + 1]);
        remotes = read_lircd_conf(argv[optind]);
	return simreceive(remotes);
}

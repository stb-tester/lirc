/**
 * Simple  tool to check that all plugins in current path can be loaded
 * Accepts a single argument identical to the --plugindir option for
 * lircd
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <fnmatch.h>

#include "paths.h"
#include "lirc_config.h"
#include "config.h"
#include "lirc_private.h"


#define USAGE \
	"\nSynopsis:\n" \
	"    lirc-lsplugins [-l] [-q] [-U plugindir] [drivers]\n" \
	"    lirc-lsplugins -e [-q] [-U plugindir]\n" \
	"    lirc-lsplugins [-s|-p|-h|-v]\n\n" \
	"If [drivers] is given list matching plugins, else list all.\n\n" \
	"Options:\n" \
	"    -U, --plugindir <path>\t" \
	"\n    \t\t\tList plugins found in ':'-separated search path.\n" \
	"    -l, --long\t\tLots of info (a. k. a. long listing).\n" \
	"    -y, --yaml\t\tGenerate a YAML plugins config file.\n" \
	"    -e, --errors\tList plugins which can't load driver(s).\n" \
	"    -s, --summary\tPrint summary on plugins status.\n" \
	"    -q, --quiet\t\tBe less verbose.\n" \
	"    -p, --default-path\tPrint default search path and exit.\n" \
	"    -h, --help\t\tDisplay this message and exit.\n" \
	"    -v, --version:\tDisplay version and exit.\n"

#define LEGEND \
	"# Flags:\n" \
	"# E:\tEmpty: Plugin loaded OK, but is empty (is this a plugin?).\n" \
	"# F:\tFail: Plugin failed to load (unresolved references?).\n" \
	"# a:\tAny: Driver can be used with any remote or capture device.\n" \
	"# s:\tSend: The driver can send data.\n"

#define LONG_LEGEND \
	"# Feature flags (see lirc(4)):\n" \
	"#    R: LIRC_CAN_SEND_RAW\t\tr: LIRC_CAN_REC_RAW\n" \
	"#    P: LIRC_CAN_SEND_PULSE\t\tp: LIRC_CAN_REC_PULSE\n" \
	"#    M: LIRC_CAN_SEND_MODE2\t\tm: LIRC_CAN_REC_MODE2\n" \
	"#    L: LIRC_CAN_SEND_LIRCCODE \t\tl: LIRC_CAN_REC_LIRCCODE\n" \
	"#    c: LIRC_CAN_SET_SEND_CARRIER\n" \
	"#    d: LIRC_CAN_SET_SEND_DUTY_CYCLE\n" \
	"#    t: LIRC_CAN_SET_TRANSMITTER_MASK\n" \
	"#    C: LIRC_CAN_MEASURE_CARRIER\n" \
	"#    D: LIRC_CAN_NOTIFY_DECODE\n"

const struct option options[] = {
	{ "plugindir",	  required_argument, NULL, 'U' },
	{ "quiet",	  no_argument,	     NULL, 'q' },
	{ "long",	  no_argument,	     NULL, 'l' },
	{ "errors",	  no_argument,	     NULL, 'e' },
	{ "summary",	  no_argument,	     NULL, 's' },
	{ "yaml",	  no_argument,	     NULL, 'y' },
	{ "default-path", no_argument,	     NULL, 'p' },
	{ "version",	  no_argument,	     NULL, 'v' },
	{ "help",	  no_argument,	     NULL, 'h' }
};


#define CAN_SEND  \
	(LIRC_CAN_SEND_RAW | LIRC_CAN_SEND_PULSE | LIRC_CAN_SEND_MODE2)
#define CAN_ANY  \
	(LIRC_CAN_REC_RAW | LIRC_CAN_REC_PULSE | LIRC_CAN_REC_MODE2)


typedef struct {
	const char*	path;
	const char*	name;
	const char*	flags;
	const char*	errors;
	const char*	features;
	const char*	version;
	const char*	info;
	const char*	device;
	const char*	device_hint;
	const char*     type;
	const char*     can_send;
} line_t;

static const line_t* lines[MAX_PLUGINS];
static int line_ix = 0;

static int opt_quiet = 0;               /**< --quiet option */
static int opt_long = 0;                /**< --long option */
static int opt_summary = 0;             /**< --summary option */
static int opt_listerrors = 0;          /**< --errors option */
static int opt_yaml = 0;                /**< --yaml option */

static int sum_drivers = 0;
static int sum_plugins = 0;
static int sum_errors = 0;


static char get(int feature, char code, const struct driver* drv)
{
// Return '-' or code depending on if features has feature x.
	return feature & drv->features ? code : '-';
}


static void  lines_next(line_t* line)
{
	lines[line_ix++] = line;
	if (line_ix >= MAX_PLUGINS - 1) {
		fputs("Too many plugins, giving up. Sorry.", stderr);
		exit(2);
	}
}


static line_t* line_new(const char* path)
// Create a new, malloc'd line.
{
	line_t* line = (line_t*) malloc(sizeof(line_t));

	line->flags = line->name = "-";
	line->path = strdup(path);
	line->errors = NULL;
	line->info = NULL;
	line->version = NULL;
	line->device = NULL;
	line->features = opt_long ? "              " : "";
	line->device_hint = NULL;
	return line;
}


static int line_cmp(const void* arg1, const void* arg2)
// qsort compare function for line_t.
{
	char key1[255];
	char key2[255];

	line_t* l1 = *((line_t**)arg1);
	line_t* l2 = *((line_t**)arg2);

	snprintf(key1, sizeof(key1), "%s%s", l1->path, l1->name);
	snprintf(key2, sizeof(key2), "%s%s", l2->path, l2->name);
	return strcmp(key1, key2);
}


static void line_print(const line_t* line)
// Print line on stdout.
{
	printf("%-20s%-6s%s\n",
	       line->name, line->flags, line->path);
	if (line->errors)
		fputs(line->errors, stdout);
}

static void print_folded_item(const char* arg)
{
	static const int START_POS = 16;
	int pos = START_POS;
	char* buff;
	char* token;

	if (arg == NULL) {
		puts("None");
		return;
	}
	buff = (char*) alloca(strlen(arg) + 1);
	strcpy(buff, arg);
	token = strtok(buff, " \t");
	while (token != NULL) {
		if (strlen(token) + pos > 80) {
			fputs("\n\t\t", stdout);
			pos = 0;
		}
		if (pos != START_POS && pos != 0) {
			fputs(" ", stdout);
			pos += 1;
		}
		fputs(token, stdout);
		pos += strlen(token);
		token = strtok(NULL, " \t");
	}
	puts("");
}

static void line_print_yaml(const line_t* line)
// Print line as a yaml 'driver' definition.
{
        size_t src;
	char next;

	if (line->errors){
		fputs("\n", stdout);
		fputs(line->errors, stdout);
		return;
	}
	printf("\n    %s:\n", line->name);
	printf("        %-16s%s\n", "type:", line->type );
	printf("        %-16s%s\n", "can_send:", line->can_send);
	printf("        %-16s'%s'\n",

	       "info:", line->info ? line->info : "None");
	if (!line->device_hint) {
		printf("        %-16s%s\n", "device_hint:","None");
	} else {
		printf("        %-16s\n", "device_hint: |");
		fputs("            \"", stdout);
		for(src = 0; line->device_hint[src] != '\0'; src += 1) {
			next = line->device_hint[src];
			if (next == '\n' && src != 0)
				printf("\"\n            \"");
			else if (next == '"')
				printf("\\\"");
			else if (next == '\\')
				printf("\\\\");
			else
				putc(next, stdout);
		}
		fputs("\"\n", stdout);
	}
}


static void line_print_long(const line_t* line)
// Print line on stdout, --long version.
{
	const char* loadstate;
	const char* handles_timing;
	const char* can_send;

	switch (line->flags[0]) {
	case '-':
		loadstate = "OK";
		break;
	case 'E':
		loadstate = "Error (unresolved dependencies?)";
		break;
	case 'F':
		loadstate = "Failed (is this a driver?)";
		break;
	default:
		loadstate = "?";
		break;
	}
	switch (line->flags[1]) {
	case '-':
		handles_timing = "No";
		break;
	case 'a':
		handles_timing = "Yes";
		break;
	default:
		handles_timing = "?";
		break;
	}
	switch (line->flags[2]) {
	case '-':
		can_send = "No";
		break;
	case 's':
		can_send = "Yes";
		break;
	default:
		can_send = "?";
		break;
	}

	printf("Plugin path:\t%s\n", line->path);
	printf("Driver name:\t%s\n", line->name ? line->name : "-");
	printf("Default device:\t%s\n", line->device ? line->device : "-");
	printf("Load state:\t%s\n", loadstate);
	printf("Timing info:\t%s\n", handles_timing);
	printf("Can send:\t%s\n", can_send);
	printf("Capabilities:\t%s\n", line->features);
	printf("Version:\t%s\n", line->version ? line->version : "(None)");
	printf("Driver info:\t");
	print_folded_item(line->info);
	puts("");
}


static void format_features(struct driver* hw, line_t* line)
{
	char buff[256];

	snprintf(buff, sizeof(buff),
		 "%c%c%c%c%c%c%c%c%c%c%c%c%c ",
		 get(LIRC_CAN_SEND_RAW, 'R', hw),
		 get(LIRC_CAN_SEND_PULSE, 'P', hw),
		 get(LIRC_CAN_SEND_MODE2, 'M', hw),
		 get(LIRC_CAN_SEND_LIRCCODE, 'L', hw),
		 get(LIRC_CAN_REC_RAW, 'r', hw),
		 get(LIRC_CAN_REC_PULSE, 'p', hw),
		 get(LIRC_CAN_REC_MODE2, 'm', hw),
		 get(LIRC_CAN_REC_LIRCCODE, 'l', hw),
		 get(LIRC_CAN_SET_SEND_CARRIER, 'c', hw),
		 get(LIRC_CAN_SET_SEND_DUTY_CYCLE, 'd', hw),
		 get(LIRC_CAN_SET_TRANSMITTER_MASK, 't', hw),
		 get(LIRC_CAN_MEASURE_CARRIER, 'C', hw),
		 get(LIRC_CAN_NOTIFY_DECODE, 'D', hw)
		 );
	line->features = strdup(buff);
}


/* Format normal lines where driver is loaded OK. */
static void format_drivers(struct driver**	drivers,
			   line_t*		line,
			   const char*		path,
			   const char*		which)
{
	char buf[1024];
	const char* what;

	if (!drivers)
		return;
	while (*drivers) {
		sum_drivers += 1;
		if (fnmatch(which, (*drivers)->name, FNM_CASEFOLD) != 0) {
			drivers++;
			continue;
		}
		if ((*drivers)->name) {
			strncpy(buf, (*drivers)->name, sizeof(buf) - 1);
			line->name = strdup(buf);
		}
		if ((*drivers)->driver_version) {
			strncpy(buf,
				(*drivers)->driver_version, sizeof(buf) - 1);
			line->version = strdup(buf);
		}
		if ((*drivers)->info) {
			strncpy(buf, (*drivers)->info, sizeof(buf) - 1);
			line->info = strdup(buf);
		}
		if ((*drivers)->device) {
			strncpy(buf, (*drivers)->device, sizeof(buf) - 1);
			line->device = strdup(buf);
		}
		if ((*drivers)->api_version >= 3 && (*drivers)->device_hint){
			strncpy(buf, (*drivers)->device_hint, sizeof(buf) - 1);
			line->device_hint = strdup(buf);
		}
		what = ((*drivers)->features & CAN_ANY) ? "code" : "mode2";
		line->type = what;
		what = ((*drivers)->features & CAN_SEND) ? "yes" : "no";
		line->can_send = what;
		snprintf(buf, sizeof(buf), "-%c%c",
			 get(CAN_ANY, 'a', *drivers),
			 get(CAN_SEND, 's', *drivers));
		line->flags = strdup(buf);
		format_features(*drivers, line);
		lines_next(line);
		drivers++;
		if (*drivers)
			line = line_new(path);
	}
}


struct driver* format_plugin(const char* path, drv_guest_func f, void* arg)
{
// Format all drivers found in plugin + load errors, arg to for_each_plugin().
	const char* which = (const char*)arg;
	void* handle;
	struct driver** drivers;
	char buffer[128];
	line_t* line;

	(void)dlerror();
	line = line_new(path);
	handle = dlopen(path, RTLD_NOW);
	sum_plugins += 1;
	if (handle != NULL) {
		drivers = (struct driver**)dlsym(handle, "hardwares");
		if (drivers != NULL && !opt_listerrors) {
			format_drivers(drivers, line, path, which);
			return NULL;
		} else if (drivers == NULL) {
			sum_errors += 1;
			line->flags = "E--";
		} else if (opt_listerrors) {
			return NULL;
		}
	} else {
		sum_errors += 1;
		line->flags = "F--";
		snprintf(buffer, sizeof(buffer), "# Error: %s\n", dlerror());
		line->errors = strdup(buffer);
	}
	lines_next(line);
	return NULL;
}


static void print_header(void)
{
	line_t line = { "Plugin", "# Driver ", "Flags" };

	line.features = opt_long ?  "Features      " : "";
	line_print(&line);
}


static void print_yaml_header(void)
{
	static const char* const YAML_HEADER =
		"#\n# Generated by lirc-lsplugins --yaml (%s) at %s#\n ";
	const time_t now = time(NULL);

	printf(YAML_HEADER, VERSION, ctime(&now));
	printf("\ndrivers:\n");
}


void lsplugins(const char* pluginpath, const char* which)
{
	for_each_plugin(format_plugin, (void*)which, pluginpath);
	qsort(lines, line_ix, sizeof(line_t*), line_cmp);
	if (opt_summary) {
		printf("Plugins: %d\n", sum_plugins);
		printf("Drivers: %d\n", sum_drivers);
		printf("Errors: %d\n", sum_errors);
		return;
	}
	if (opt_yaml)
		print_yaml_header();
	else if (!opt_quiet && !opt_long)
		print_header();

	for (int i = 0; i < line_ix; i++) {
		if (opt_yaml)
			line_print_yaml(lines[i]);
		else if (opt_long)
			line_print_long(lines[i]);
		else
			line_print(lines[i]);
	}
	if (!opt_quiet) {
		puts("#\n#");
		if (!opt_long) {
			fputs(LEGEND, stdout);
			fputs("#", stdout);
		}
		if (opt_long)
			puts(LONG_LEGEND);
	}
}


int main(int argc, char** argv)
{
	const char* pluginpath;
	char path [128];
	const char* which;
	int c;
	const loglevel_t level = options_get_app_loglevel("lirc-lsplugins");

	pluginpath = LIBDIR "/lirc/plugins";
	if (getenv(PLUGINDIR_VAR) != NULL)
		pluginpath = getenv(PLUGINDIR_VAR);
	while ((c = getopt_long(argc, argv,
				"selpqvhU:y", options, NULL)) != -1) {
		switch (c) {
		case 'U':
			pluginpath = optarg;
			break;
		case 'h':
			puts(USAGE);
			exit(0);
		case 'v':
			puts("Version: " VERSION "\n");
			exit(0);
		case 'p':
			printf("Default path: %s\n", pluginpath);
			exit(0);
		case 'e':
			opt_listerrors = 1;
			break;
		case 's':
			opt_summary = 1;
			break;
		case 'q':
			opt_quiet = 1;
			break;
		case 'l':
			opt_long = 1;
			break;
		case 'y':
			opt_yaml = 1;
			opt_quiet = 1;
			break;
		default:
			fputs(USAGE, stderr);
			exit(1);
		}
	}
	if (argc - optind > 1) {
		fputs("Too many arguments.\n", stderr);
		fputs(USAGE, stderr);
		exit(2);
	}
	which = (argc - optind == 1 ?  argv[argc - 1] : "*");

	lirc_log_get_clientlog("lirc-lsplugins", path, sizeof(path));
	unlink(path);
	lirc_log_set_file(path);
	lirc_log_open("lirc-lsplugins", 1, level);

	lsplugins(pluginpath, which);
	return sum_errors == 0 ? 0 : 1;
}

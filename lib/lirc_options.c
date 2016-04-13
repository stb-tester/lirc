/****************************************************************************
** lirc_options.c **********************************************************
****************************************************************************
*
* options.c - global options access.
*
*/

/**
 * @file lirc_options.c
 * @brief  Implements lirc_options.h.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__)
#include <linux/types.h>
#endif

#include "ciniparser.h"
#include "lirc_options.h"
#include "lirc_log.h"

static const logchannel_t logchannel = LOG_LIB;

dictionary* lirc_options = NULL;

/* Environment variable which if set enables some debug output. */
static const char* const LIRC_DEBUG_OPTIONS = "LIRC_DEBUG_OPTIONS";

static int depth = 0;

static int options_debug = -1;

loglevel_t options_set_loglevel(const char* optarg)
{
	char s[4];
	loglevel_t level;

	level = string2loglevel(optarg);
	if (level == LIRC_BADLEVEL)
		return level;
	snprintf(s, sizeof(s), "%d", level);
	options_set_opt("lircd:debug", s);
	return level;
}


void options_set_opt(const char* key, const char* value)
{
	if (dictionary_set(lirc_options, key, value) != 0)
		log_warn("Cannot set option %s to %s\n", key, value);
}


const char* options_getstring(const char* const key)
{
	return ciniparser_getstring(lirc_options, key, 0);
}


int options_getint(const char* const key)
{
	return ciniparser_getint(lirc_options, key, 0);
}


int  options_getboolean(const char* const key)
{
	return ciniparser_getboolean(lirc_options, key, 0);
}

static const struct option o_option[] = {
	{ "options-file", required_argument, NULL, 'O' },
	{ 0,		  0,		     0,	   0   }
};


static char* parse_O_arg(int argc, char** argv)
{
	char* path = NULL;
	int i;

	for (i = 0; i < argc; i += 1) {
		if (strcmp(argv[i], "-O") != 0 &&
		    strcmp(argv[i], "--options-file") != 0)
			continue;
		if (i + 1 >= argc)
			return NULL;
		path = argv[i + 1];
		if (path && access(path, R_OK) != 0) {
			fprintf(stderr, "Cannot open options file %s for read\n",
				path);
			return NULL;
		}
		return path;
	}
	return NULL;
}


void options_load(int argc, char** const argv,
		  const char* path_arg,
		  void (*parse_options)(int, char** const))
{
	char buff[128];
	char buff2[128];
	const char* path = path_arg;

	if (depth > 1) {
		log_warn("Error:Cowardly refusing to process"
			  " options-file option within a file\n");
		return;
	}
	depth += 1;
	setenv("POSIXLY_CORRECT", "1", 1);
	if (path == NULL)
		path = parse_O_arg(argc, argv);
	if (path == NULL) {
		path = getenv(LIRC_OPTIONS_VAR);
		path = (path == NULL ? LIRC_OPTIONS_PATH : path);
	}
	if (*path != '/') {
		if (getcwd(buff2, sizeof(buff2)) == NULL)
			log_perror_warn("options_load: getcwd():");
		snprintf(buff, sizeof(buff), "%s/%s", buff2, path);
		path = buff;
	}
	if (access(path, R_OK) == 0) {
		lirc_options = ciniparser_load(path);
		if (lirc_options == NULL) {
			log_warn("Cannot load options file %s\n", path);
			lirc_options = dictionary_new(0);
		}
	} else {
		fprintf(stderr, "Warning: cannot open %s\n", path);
		log_warn("Cannot open %s\n", path);
		lirc_options = dictionary_new(0);
	}
	if (parse_options != NULL)
		parse_options(argc, argv);
	if (options_debug == -1)
		options_debug = getenv(LIRC_DEBUG_OPTIONS) != NULL;
	if (options_debug && lirc_options != NULL) {
		fprintf(stderr, "Dumping parsed option values:\n");
		ciniparser_dump(lirc_options, stderr);
	}
}


loglevel_t options_get_app_loglevel(const char* app)
{
	const char* s;
	loglevel_t level = LIRC_BADLEVEL;
	char buff[64];

	s = getenv("LIRC_LOGLEVEL");
	level = string2loglevel(s);
	if (level != LIRC_BADLEVEL)
		return level;
	if (lirc_options == NULL)
		options_load(0, NULL, NULL, NULL);
	if (level == LIRC_BADLEVEL && app != NULL) {
		snprintf(buff, sizeof(buff), "%s:debug", app);
		s = ciniparser_getstring(lirc_options, buff, NULL);
		level = string2loglevel(s);
	}
	if (level == LIRC_BADLEVEL) {
		s = ciniparser_getstring(lirc_options, "lircd:debug", "debug");
		level = string2loglevel(s);
		if (level == LIRC_BADLEVEL)
			level = LIRC_DEBUG;
	}
	return level;
}


void options_add_defaults(const char* const defaults[])
{
	int i;
	const char* key;
	const char* value;

	for (i = 0; defaults[i] != NULL; i += 2) {
		key = defaults[i];
		value = defaults[i + 1];
		if (ciniparser_getstring(lirc_options, key, NULL) == NULL)
			options_set_opt((char*)key, (char*)value);
	}
}

void options_unload(void)
{
	depth = 0;
	options_debug = -1;
	if (lirc_options != NULL) {
		dictionary_del(lirc_options);
		lirc_options = NULL;
	}
}

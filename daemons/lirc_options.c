/****************************************************************************
 ** lirc_options.c **********************************************************
 ****************************************************************************
 *
 * options.c - global options access.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/types.h>

#include "ciniparser.h"
#include "lirc_options.h"
#include "lirc_log.h"

dictionary* lirc_options = NULL;

static int depth = 0;

#ifdef DEBUG
extern int debug;
#endif


void options_set_opt(char* key, char* value)
{
	if (dictionary_set(lirc_options, key, value) != 0)
		logprintf(LOG_WARNING,
			  "Cannot set option %s to %s\n", key, value);
}


char* options_getstring(char* key)
{
	return ciniparser_getstring(lirc_options, key, 0);
}


int options_getint(char* key)
{
	return ciniparser_getint(lirc_options, key, 0);
}


int  options_getboolean(char* key)
{
	return ciniparser_getboolean(lirc_options, key, 0);
}


void options_load(int argc, char** argv,
		  const char* path,
		  void(*parse_options)(int, char**))
{
	char buff[128];

	if (depth > 1) {
		logprintf(LOG_WARNING,
			 "Error:Cowardly refusing to process"
			     " options-file option within a file\n");
		return;
	}
	depth += 1;
	if (path == NULL) {
		path = getenv( LIRC_OPTIONS_VAR );
		path = (path == NULL ? LIRC_OPTIONS_PATH : path);
	}
	if (*path != '/') {
		snprintf(buff, sizeof(buff),
			 "%s/lirc/%s", SYSCONFDIR, path);
		path = buff;
	}
	if (access(path, R_OK) != 0) {
		lirc_options = ciniparser_load(path);
		if (lirc_options == NULL) {
			logprintf(LOG_WARNING,
				 "Cannot load options file %s\n", path);
			lirc_options = dictionary_new(0);
		}
	}
        else {
		logprintf(LOG_WARNING, "Warning: cannot open %s\n", path);
		lirc_options = dictionary_new(0);
        }
	parse_options(argc, argv);
#	ifdef DEBUG
	if (debug && lirc_options != NULL ) {
		fprintf(stderr, "Dumping parsed option values:\n" );
		ciniparser_dump(lirc_options, stdout);
	}
#	endif
}


void options_add_defaults(const char* const defaults[])
{
	int i;
	const char* key;
	const char* value;

	for(i = 0; defaults[i] != NULL; i += 2){
		key = defaults[i];
		value = defaults[i + 1];
		if (ciniparser_getstring(lirc_options, key, NULL) == NULL)
			options_set_opt((char*)key, (char*)value);
	}
}

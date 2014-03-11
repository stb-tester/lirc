/****************************************************************************
 ** options.h ***************************************************************
 ****************************************************************************
 *
 * options.h - global options access.
 *
 */
#include "ciniparser.h"

/* Global options instance with all option values. */
extern dictionary* lirc_options;

/* Set given option to value (always a string). */
void options_set_opt(char* key, char* value);

/* Get a [string|int|boolean] option with 0 as default value. */
char* options_getstring(char* key);
int options_getint(char* key);
int options_getboolean(char* key);


/*
 * Set unset options using values in defaults list.
 * Arguments:
 *   - defaults: NULL-terminated list of key, value [, key, value]...
 */
void options_add_defaults(const char* const defaults[]);


/*
 *   Parse global option file and command line. On exit, all values
 *   are set, possibly to defaults.
 *   Arguments:
 *      - argc, argv; As handled to main()
 *      - options-file: Path to options file. If NULL, the default one
 *        will be used.
 *      - options_load: Function called as options_load(argc, argv, path).
 *        argc and argv are as given to options_init; path is the absolute
 *        path to the configuration file.
 *
 */
void options_load(int argc, char **argv,
		  const char* options_file,
		  void (*options_load)(int, char**));

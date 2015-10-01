/****************************************************************************
** options.h ***************************************************************
****************************************************************************/

/**
 * @file lirc_options.h
 * @brief Options management: options file, parse and retrieve.
 *
 * @ingroup private_api
 */

#ifndef LIRC_OPTIONS
#define LIRC_OPTIONS

#ifdef __cplusplus
extern "C" {
#endif

#include "lirc_log.h"
#include "ciniparser.h"

/* Global options instance with all option values. */
extern dictionary* lirc_options;

/* Set given option to value (always a string). */
void options_set_opt(const char* key, const char* value);

/** Parse and store a loglevel, returning value (possibly LIRC_BADLEVEL). */
loglevel_t options_set_loglevel(const char* optarg);

/**
 * Return loglevel based on (falling priority)
 *  - LIRC_LOGLEVEL in environment,
 *  - If app is non-NULL the 'debug' value in the [app]section
 *  - The 'debug' value in the [lircd] options file section.
 *  - The hardcoded default LIRC_DEBUG
 */
loglevel_t options_get_app_loglevel(const char* app);

/* Get a [string|int|boolean] option with 0 as default value. */
const char* options_getstring(const char* const key);
int options_getint(const char* const key);
int options_getboolean(const char* const key);


/*
 * Set unset options using values in defaults list.
 * Arguments:
 *   - defaults: NULL-terminated list of key, value [, key, value]...
 */
void options_add_defaults(const char* const defaults[]);


/*
 *   Parse global option file and command line. On exit, all values
 *   @ingroup  private_api
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
void options_load(int argc,
		  char** const argv,
		  const char* options_file,
		  void (*options_load)(int, char** const));


/* Reset options to pristine state. */
void options_unload(void);

#ifdef __cplusplus
}
#endif

#endif

/**
 *  @file drv_admin.h
 *  @author Alec Leamas
 *  @date August 2014
 *  @license GPL2 or later
 *  @brief Routines for dynamic drivers.
 *  @ingroup private_api
 *
 *  Functions in this file provides primitives to iterate over
 *  the dynamic drivers + a single function to install such a driver.
 *
 *  Drivers are loaded from a path defined by (falling priority):
 *
 *    - The "lircd:pluginpath" option.
 *    - The LIRC_PLUGIN_PATH environment variable.
 *    - The hardcoded PLUGINDIR constant.
 */

#include "driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * Argument to for_each_driver(). Called with the loaded struct driver*
 * data and the argument given to for_each_driver(). Returns NULL if
 * iteration should continue, else a struct hardware* pointer.
 *
 */
typedef struct driver* (*drv_guest_func)(struct driver*, void*);

/**
 * Argument to for_each_plugin. Called with a path to the so-file,
 * a function to apply to each found driver (see drv_guest_func()) and
 * an untyped argument given to for_each_plugin(). Returns NULL if
 * iteration should continue, else a struct driver* pointer.
 */
typedef struct driver*
(* plugin_guest_func)(const char*, drv_guest_func, void*);

/**
 * Search for driver with given name, update global drv with driver data if found.
 *
 * @return Returns 0 if found and hw updated, else -1.
 */
int hw_choose_driver(const char* name);

/* Print name of all drivers on FILE. */
void hw_print_drivers(FILE*);

/**
 *
 * Apply func to all existing drivers. Returns pointer to a driver
 * if such a pointer is returned by func(), else NULL.
 * Pluginpath defaults to lircd:pluginpath, LIRC_PLUGINPATH and
 * a hardcoded last resort.
 *
 */
struct driver* for_each_driver(drv_guest_func func,
			       void* arg,
			       const char* pluginpath);

/**
 * Apply func to all plugins (i. e., .so-files) in plugin path.
 * plugin path default to lircd:pluginpath, LIRC_PLUGINPATH and
 * a hardcoded last resort.
 */
void for_each_plugin(plugin_guest_func plugin_guest,
		     void* arg,
		     const char* pluginpath);


#ifdef __cplusplus
}
#endif

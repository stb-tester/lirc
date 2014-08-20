/**
 *  @file hw-types.h
 *  @author Alec Leamas
 *  @date August 2014
 *  @copyright GPL2 or later
 *  @brief Routines for dynamic drivers.
 */

//#include "lirc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware types */
extern struct driver hw;
int hw_choose_driver(const char* name);
void hw_print_drivers(FILE*);

#define PLUGIN_FILE_EXTENSION "so"

#ifdef __cplusplus
}
#endif

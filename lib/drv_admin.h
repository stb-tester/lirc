/**
 *  @file drv_admin.h
 *  @author Alec Leamas
 *  @date August 2014
 *  @license GPL2 or later
 *  @brief Routines for dynamic drivers.
 */

//#include "lirc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Argument to for_each_driver. Called with a driver and
 * an argument given to for_each_driver. Returns NULL if
 * iteration should continue, else a defined pointer to a
 * driver struct
 */
typedef struct driver* (*drv_guest_func)(struct driver*, void*);

/* The currently active driver. */
extern struct driver drv;

/* Search for driver with name and install it in the drv struct. */
int hw_choose_driver(const char* name);

/* Print name of all drivers on FILE. */
void hw_print_drivers(FILE*);

/*
 * Apply func to all existing drivers. Returns pointer to a driver
 * if such  a pointer is returned by func(), else NULL.
 *
 */
struct driver* for_each_driver(drv_guest_func func, void* arg);

#define PLUGIN_FILE_EXTENSION "so"

#ifdef __cplusplus
}
#endif

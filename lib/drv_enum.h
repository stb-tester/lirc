#ifndef _DRV_ENUM_H
#define _DRV_ENUM_H
/**
 *  @file drv_enum.h
 *  @author Alec Leamas
 *  @date August 2014
 *  @ense GPL2 or later
 *  @brief dynamic drivers device enumeration support
 *  @ingroup driver_api
 *
 *  Functions in this file provides support for enumerating devices
 *  i. e., DRVCTL_GET_DEVICES.
 */

#include "driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Free memory obtained using DRVCTL_GET_DEVICES.
 */
void drv_enum_free(glob_t* glob);


/** list all devices matching pattern (just a glob() wrapper). */
glob_t*  drv_enum_glob(glob_t* glob, const char* pattern);

#ifdef __cplusplus
}
#endif

#endif   // _DRV_ENUM_H

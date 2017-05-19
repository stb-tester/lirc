#ifndef _DRV_ENUM_H
#define _DRV_ENUM_H
/**
 *  @brief dynamic drivers device enumeration support
 *  @file drv_enum.h
 *  @author Alec Leamas
 *  @license GPL2 or later
 *  @date December 2016
 *  @ingroup driver_api
 *
 *  Functions in this file provides support for enumerating devices
 *  i. e., DRVCTL_GET_DEVICES. If libudev is available, all functions
 *  adds udev info to the output.
 *
 *  All drv_enum functions returns data in a glob_t* with matched devices in
 *  gl_pathv, one device per entry. The first word in each entry is
 *  the mandatory device path. The optional remainder is more info
 *  on device, usable in user interfaces.
 *
 *  Return codes are DRV_ERR_ constants as of driver.h, or 0 for no errors.
 */

#include <stdint.h>

#include "driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Condition to match in drv_enum_udev(). Null fields are ignored. */
struct drv_enum_udev_what  {
	const char* idVendor;
	const char* idProduct;
	const char* subsystem;      /**< Require given subsystem. */
	const char* parent_subsys;  /**< Require a given subsystem parent. */
};

/** Setup a glob_t variable to empty state. */
void glob_t_init(glob_t* glob);

/** Add a path to glob, allocating memory as necessary. */
void glob_t_add_path(glob_t* glob, const char* path);

/** Free memory obtained using any of the drv_enum_* functions  */
void drv_enum_free(glob_t* glob);

/**
 * Try to add udev info to existing entries in glob. Existing
 * info besides the device path is discarded.
 */
void drv_enum_add_udev_info(glob_t* glob);

/** List all devices matching glob(3) pattern. */
int drv_enum_glob(glob_t* glob, const char* pattern);

/** List devices matching any of patterns in null-terminated list. */
int drv_enum_globs(glob_t* globbuf, const char* const* patterns);

/** List all devices matching any of conditions in {0}-terminated list. */
int drv_enum_udev(glob_t* globbuf,
		  const struct drv_enum_udev_what* what);

/** List all available devices matched by is_device_ok() using libusb.  */
int drv_enum_usb(glob_t* glob,
		 int (*is_device_ok)(uint16_t vendor,  uint16_t product));


#ifdef __cplusplus
}
#endif

#endif   // _DRV_ENUM_H

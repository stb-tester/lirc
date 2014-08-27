/**
 *  @file driver.c
 *  @author Alec Leamas
 *  @date August 2014
 *  @license GPL2 or later
 *
 * Access and support for driver.h, the global driver.
 */

#include 	"driver.h"

/**
 * The global driver data that drivers etc are accessing.
 * Set by hw_choose_driver().
 */
struct driver drv;

/** Read-only access to drv for client code. */
const struct driver const* curr_driver = &drv;

int default_open(const char* path)
{
	static char buff[128];
	strncpy(buff, path, sizeof(buff));
	drv.device = buff;
	return 0;
}

int default_close()
{
	return 0;
}

int default_drvtcl(int fd, void* arg)
{
	return DRV_ERR_NOT_IMPLEMENTED;
}


/**
 *  @file driver.c
 *  @author Alec Leamas
 *  @date August 2014
 *  @license GPL2 or later
 *
 * Access and support for driver.h, the global driver.
 */

#include 	"driver.h"

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


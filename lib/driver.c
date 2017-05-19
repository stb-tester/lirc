/**
 *  @brief Implements driver.h
 *  @file driver.c
 *  @author Alec Leamas
 *  @license GPL2 or later
 *  @date August 2014
 *
 * Access and support for driver.h, the global driver.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include	<stdio.h>
#include	"driver.h"
#include	"config.h"
#include	"lirc_log.h"

static const logchannel_t logchannel = LOG_LIB;

int get_server_version(void) { return VERSION_NODOTS; }

/**
 * The global driver data that drivers etc are accessing.
 * Set by hw_choose_driver().
 */
struct driver drv;

/** sscanf format to parse option_t. */
const char* const OPTION_FMT = "%32s%64s";

/** Read-only access to drv for client code. */
const struct driver* const curr_driver = &drv;


int default_open(const char* path)
{
	static char buff[128];

	if (path == NULL) {
		if (drv.device == NULL)
			drv.device = LIRC_DRIVER_DEVICE;
	} else {
		strncpy(buff, path, sizeof(buff) - 1);
		drv.device = buff;
	}
	log_info("Initial device: %s", drv.device);
	return 0;
}

int default_close(void)
{
	return 0;
}

int default_drvctl(unsigned int fd, void* arg)
{
	return DRV_ERR_NOT_IMPLEMENTED;
}


int drv_handle_options(const char* options)
{
	char* s;
	char* token;
	struct option_t option;
	int found;
	char* colon;
	int result;

	if (options == NULL || strlen(options) == 0)
		return 0;
	s = alloca(strlen(options) + 1);
	strcpy(s, options);
	for (token = strtok(s, "|"); token != NULL; token = strtok(NULL, "|")) {
		colon = strstr(token, ":");
		if (colon == NULL)
			return DRV_ERR_BAD_OPTION;
		*colon = ' ';
		found = sscanf(token, OPTION_FMT, option.key, option.value);
		if (found != 2)
			return DRV_ERR_BAD_OPTION;
		if (!curr_driver->drvctl_func)
			continue;
		result = curr_driver->drvctl_func(DRVCTL_SET_OPTION, (void*) &option);
		if (result != 0)
			return result;
	}
	return 0;
}

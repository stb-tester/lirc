
/****************************************************************************
 * util.c ***************************************************************
 ****************************************************************************
 */

/**
 * @file util.c
 * @author Alec Leamas
 * @brief Utilities.
 */

#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "lirc/lirc_log.h"
#include "util.h"


const char* drop_sudo_root(int (*set_some_uid)(uid_t))
{
	struct passwd* pw;
	char* user;
	int r;

	if (getuid() != 0) {
		return "";
	}
	user = getenv("SUDO_USER");
	if (user == NULL) {
		return "root";
	}
	pw = getpwnam(user);
	if (pw == NULL) {
		logperror(LIRC_ERROR, "Can't run getpwnam() for %s", user);
		return "";
	}
	r = set_some_uid(pw->pw_uid);
	if (r == -1) {
		logperror(LOG_WARNING, "Cannot change UID to %d", pw->pw_uid);
		return "";
	}
	setenv("HOME", pw->pw_dir, 1);
	logprintf(LOG_NOTICE, "Running as user %s", pw->pw_name);
	return pw->pw_name;
}




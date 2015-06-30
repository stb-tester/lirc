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
#include <string.h>
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

	if (getuid() != 0)
		return "";
	user = getenv("SUDO_USER");
	if (user == NULL)
		return "root";
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


void drop_root_cli(int (*set_some_uid)(uid_t))
{
	const char* new_user;

	new_user = drop_sudo_root(set_some_uid);
	if (strcmp("root", new_user) == 0)
		puts("Warning: Running as root.");
	else if (strlen(new_user) == 0)
		puts("Warning: Cannot change uid.");
	else
		printf("Running as regular user %s\n", new_user);
}

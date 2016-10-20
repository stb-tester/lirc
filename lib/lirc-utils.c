/****************************************************************************
 * util.c ***************************************************************
 ****************************************************************************
 */

/**
 * @file util.c
 * @author Alec Leamas
 * @brief Utilities.
 */

#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "lirc/lirc_log.h"
#include "lirc-utils.h"

static const logchannel_t logchannel = LOG_LIB;

#ifdef HAVE_INT_GETGROUPLIST_GROUPS
#define lirc_gid int
#else
#define lirc_gid gid_t
#endif


const char* drop_sudo_root(int (*set_some_uid)(uid_t))
{
	struct passwd* pw;
	char* user;
	lirc_gid groups[32];
	int group_cnt = sizeof(groups)/sizeof(gid_t);
	char groupnames[256] = {0};
	char buff[12];
	int r;
	int i;

	if (getuid() != 0)
		return "";
	user = getenv("SUDO_USER");
	if (user == NULL)
		return "root";
	pw = getpwnam(user);
	if (pw == NULL) {
		log_perror_err("Can't run getpwnam() for %s", user);
		return "";
	}
	r = getgrouplist(user, pw->pw_gid, groups, &group_cnt);
	if (r == -1) {
		log_perror_warn("Cannot get supplementary groups");
		return "";
	}
	r = setgroups(group_cnt, groups);
	if (r == -1) {
		log_perror_warn("Cannot set supplementary groups");
		return "";
	}
	r = setgid(pw->pw_gid);
	if (r == -1) {
		log_perror_warn("Cannot set GID");
		return "";
	}
	r = set_some_uid(pw->pw_uid);
	if (r == -1) {
		log_perror_warn("Cannot change UID to %d", pw->pw_uid);
		return "";
	}
	setenv("HOME", pw->pw_dir, 1);
	log_notice("Running as user %s", user);
	for (i = 0; i < group_cnt; i += 1) {
		snprintf(buff, 5, " %d", groups[i]);
		strcat(groupnames, buff);
	}
	log_debug("Groups: [%d]:%s", pw->pw_gid, groupnames);

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

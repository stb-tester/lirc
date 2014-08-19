/**
 *  @file hw-types.c
 *  @author Alec Leamas
 *  @date August 2014
 *  @copyright GPL2 or later
 *
 * Routines for dynamic drivers. This file was previously used for other purposes, and should possibly be renamed.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <dirent.h>
#include <dlfcn.h>

#include "lirc/hardware.h"
#include "lirc/hw-types.h"
#include "lirc/lirc_options.h"
#include "lirc_log.h"

/**
 * The global hardware that drivers etc are accessing.
 * Defined in hw-types.c.
 * Set by hw_choose_driver().
 */
struct hardware hw;

/** Plugin currently in use, if non-NULL */
static void* last_plugin = NULL;

/**
 * Just local usage  within this file.
 */
typedef struct hardware* (*hw_guest_func)(struct hardware*, void*);

struct hardware hw_default = {
	"/dev/null",            /* default device */
	-1,                     /* fd */
	0,                      /* features */
	0,                      /* send_mode */
	0,                      /* rec_mode */
	0,                      /* code_length */
	NULL,                   /* init_func */
	NULL,                   /* deinit_func */
	NULL,                   /* send_func */
	NULL,                   /* rec_func */
	NULL,                   /* decode_func */
	NULL,                   /* ioctl_func */
	NULL,                   /* readdata */
	"null",                 /* name */
};

/**
 * Return true if and only if str ends with ".so".
 * @param str
 * @return
 */
static int ends_with_so(const char *str)
{
    char *dot = strrchr(str, '.');

    return (NULL == dot) ? 0 : strcmp(dot + 1, PLUGIN_FILE_EXTENSION) == 0;
}

/**
 * hw_guest_func which prints name of *hw on file.
 * @param hw
 * @param file
 * @return NULL
 */
static struct hardware* print_hw_name(struct hardware* hw, void* file)
{
	fprintf((FILE*)file, "\t%s\n", hw->name);
	return NULL;
}


static struct hardware* match_hw_name(struct hardware* hw, void* name)
// hw_guest_func. Returns hw if hw->name == name, else NULL.
{
	if (hw == (struct hardware*) NULL || name == NULL )
		return (struct hardware*)NULL;
	if (strcasecmp(hw->name, (char*)name) == 0)
		return hw;
	return (struct hardware*)NULL;
}

static struct hardware*
visit_plugin(char* path, hw_guest_func func, void* arg)
// Apply func(hw, arg) for all drivers found in plugin on path.
{
	struct hardware** hardwares;
	struct hardware* result = (struct hardware*) NULL;

	(void)dlerror();
	if (last_plugin != NULL)
		dlclose(last_plugin);
	last_plugin = dlopen(path, RTLD_NOW);
	if (last_plugin == NULL) {
		logprintf(LOG_ERR, dlerror());
		return result;
	}
	hardwares = (struct hardware**)dlsym(last_plugin, "hardwares");
	if (hardwares == (struct hardware**)NULL ){
		logprintf(LOG_WARNING,
			 "No hardwares entrypoint found in %s", path);
	}
	else {
		for ( ; *hardwares; hardwares++) {
			if( (*hardwares)->name == NULL){
				logprintf(LOG_WARNING,
					  "No driver name in %s", path);
				continue;
			}
			result = (*func)(*hardwares, arg);
			if (result != (struct hardware*) NULL)
				break;
		}
	}
	return result;

}


static struct hardware*
for_each_driver_in_dir(const char* dirpath, hw_guest_func func, void* arg)
// Apply func(hw, arg) for all drivers found in all plugins in directory path
{
	DIR* dir;
	struct dirent* ent;
	struct hardware* result = (struct hardware*) NULL;
	char path[128];

	if ((dir = opendir(dirpath)) == NULL){
		logprintf(LOG_INFO, "Cannot open plugindir %s", dirpath);
		return  (struct hardware*) NULL;
	}
	while ((ent = readdir(dir)) != NULL) {
		if (!ends_with_so(ent->d_name))
			continue;
		snprintf(path, sizeof(path),
			 "%s/%s", dirpath, ent->d_name);
		result = visit_plugin(path, func, arg);
		if (result != (struct hardware*) NULL)
			break;
	}
	closedir(dir);
	return result;
}

static struct hardware* for_each_driver(hw_guest_func func, void* arg)
// Apply func(hw, arg) for all drivers found in all plugins.
{
	char* pluginpath;
	char* tmp_path;
	char* s;
	struct hardware* result = (struct hardware*) NULL;

	pluginpath = ciniparser_getstring(lirc_options,
		  			  "lircd:plugindir",
					  getenv(PLUGINDIR_VAR));
	if (pluginpath == NULL)
		pluginpath = PLUGINDIR;
        if (strchr(pluginpath, ':') == (char*) NULL)
		return for_each_driver_in_dir(pluginpath, func, arg);
	tmp_path = alloca(strlen(pluginpath) + 1);
	strncpy(tmp_path, pluginpath, strlen(pluginpath) + 1);
	for (s = strtok(tmp_path, ":"); s != NULL; s = strtok(NULL, ":")) {
		result = for_each_driver_in_dir(s, func, arg);
		if (result != (struct hardware*) NULL)
			break;
	}
	return result;
}

/**
 * @brief Prints all drivers known to the system to the file given as argument.
 * @param file File to print to.
 */
 void hw_print_drivers(FILE* file)
// Print list of all hardware names (i. e., drivers) on file.
{
	for_each_driver(print_hw_name, (void*)file);
}

/**
 * Search for driver, update global hw with driver data if found.
 *
 * @param name
 * @return Returns 0 if found and hw updated, else -1.
 */
int hw_choose_driver(char* name)
{
	struct hardware* found;

	if (name == NULL) {
		memcpy(&hw, &hw_default, sizeof(struct hardware));
		return 0;
	}
	if (strcasecmp(name, "dev/input") == 0) {
		/* backwards compatibility */
		name = "devinput";
	}
	found = for_each_driver(match_hw_name, (void*)name);
	if (found != (struct hardware*)NULL){
		memcpy(&hw, found, sizeof(struct hardware));
		hw.fd = -1;
		return 0;
	}
	return -1;
}

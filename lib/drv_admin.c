/**
 *  @file drv_admin.c
 *  @brief Implements drv_admin.h
 *  @author Alec Leamas
 *  @date August 2014
 *  license: GPL2 or later
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <dirent.h>
#include <dlfcn.h>

#include "lirc/driver.h"
#include "lirc/drv_admin.h"
#include "lirc/lirc_options.h"
#include "lirc_log.h"

#include "driver.h"

static const logchannel_t logchannel = LOG_LIB;

static const char* const PLUGIN_FILE_EXTENSION  = "so";


/** Max number if plugins handled. No point to malloc() this. */
#define MAX_PLUGINS  256

extern struct driver drv;  /**< Access to otherwise private drv.*/

/** Array of plugin names, fixed max size. */
typedef struct {
	char*	array[MAX_PLUGINS];
	int	size;
} char_array;

/** Plugin currently in use, if non-NULL */
static void* last_plugin = NULL;

/** Default driver, a placeholder. */
const struct driver drv_null = {
	.name		= "null",
	.device		= "/dev/null",
	.features	= 0,
	.send_mode	= 0,
	.rec_mode	= 0,
	.code_length	= 0,
	.init_func	= NULL,
	.deinit_func	= NULL,
	.send_func	= NULL,
	.rec_func	= NULL,
	.decode_func	= NULL,
	.readdata	= NULL,
	.drvctl_func	= default_drvctl,
	.open_func	= default_open,
	.close_func	= default_close,
	.api_version	= 2,
	.driver_version = "0.9.2"
};


/**
 * Return true if and only if str ends with ".so".
 * @param str
 * @return
 */
static int ends_with_so(const char* str)
{
	char* dot = strrchr(str, '.');

	return (NULL == dot) ? 0 : strcmp(dot + 1, PLUGIN_FILE_EXTENSION) == 0;
}


/** qsort compare function for array lines. */
static int line_cmp(const void* arg1, const void* arg2)
{
	return strcmp(*(const char**)arg1, *(const char**)arg2);
}


/** hw_guest_func which adds name of driver to array in arg. */
static struct driver* add_hw_name(struct driver* hw, void* arg)
{
	char_array* a = (char_array*)arg;

	if (a->size >= MAX_PLUGINS) {
		log_error("Too many plugins(%d)", MAX_PLUGINS);
		return hw;
	}
	a->array[a->size] = strdup(hw->name);
	a->size += 1;
	return NULL;
}


static struct driver* match_hw_name(struct driver* drv, void* name)
{
// drv_guest_func. Returns hw if hw->name == name, else NULL.
	if (drv == (struct driver*)NULL || name == NULL)
		return (struct driver*)NULL;
	if (strcasecmp(drv->name, (char*)name) == 0)
		return drv;
	return (struct driver*)NULL;
}


static struct driver*
visit_plugin(const char* path, drv_guest_func func, void* arg)
{
// Apply func(hw, arg) for all drivers found in plugin on path.
	struct driver** drivers;
	struct driver* result = (struct driver*)NULL;

	(void)dlerror();
	if (last_plugin != NULL)
		dlclose(last_plugin);
	last_plugin = dlopen(path, RTLD_NOW);
	if (last_plugin == NULL) {
		log_error(dlerror());
		return result;
	}
	drivers = (struct driver**)dlsym(last_plugin, "hardwares");
	if (drivers == (struct driver**)NULL) {
		log_warn("No hardwares entrypoint found in %s", path);
	} else {
		for (; *drivers; drivers++) {
			if ((*drivers)->name == NULL) {
				log_warn("No driver name in %s", path);
				continue;
			}
			result = (*func)(*drivers, arg);
			if (result != (struct driver*)NULL)
				break;
		}
	}
	return result;
}


/* Apply plugin_guest(path, drv_guest, arg) to all so-files in dir. */
static struct driver* for_each_plugin_in_dir(const char*	dirpath,
					     plugin_guest_func	plugin_guest,
					     drv_guest_func	drv_guest,
					     void*		arg)
{
	DIR* dir;
	struct dirent* ent;
	struct driver* result = (struct driver*)NULL;
	char path[128];
	char buff[128];

	dir = opendir(dirpath);
	if (dir == NULL) {
		log_info("Cannot open plugindir %s", dirpath);
		return (struct driver*)NULL;
	}
	while ((ent = readdir(dir)) != NULL) {
		if (!ends_with_so(ent->d_name))
			continue;
		strncpy(buff, dirpath, sizeof(buff) - 1);
		if (buff[strlen(buff) - 1] == '/')
			buff[strlen(buff) - 1] = '\0';
		snprintf(path, sizeof(path),
			 "%s/%s", buff, ent->d_name);
		result = plugin_guest(path, drv_guest, arg);
		if (result != (struct driver*)NULL)
			break;
	}
	closedir(dir);
	return result;
}


static struct driver* for_each_path(plugin_guest_func	plg_guest,
				    drv_guest_func	drv_guest,
				    void*		arg,
				    const char*		pluginpath_arg)
{
	const char* pluginpath;
	char* tmp_path;
	char* s;
	struct driver* result = (struct driver*)NULL;

	if (pluginpath_arg == NULL) {
		pluginpath = ciniparser_getstring(lirc_options,
						  "lircd:plugindir",
						  getenv(PLUGINDIR_VAR));
		if (pluginpath == NULL)
			pluginpath = PLUGINDIR;
	} else {
		pluginpath = pluginpath_arg;
	}
	if (strchr(pluginpath, ':') == (char*)NULL) {
		return for_each_plugin_in_dir(pluginpath,
					      plg_guest,
					      drv_guest,
					      arg);
	}
	tmp_path = alloca(strlen(pluginpath) + 1);
	strncpy(tmp_path, pluginpath, strlen(pluginpath) + 1);
	for (s = strtok(tmp_path, ":"); s != NULL; s = strtok(NULL, ":")) {
		result = for_each_plugin_in_dir(s,
						plg_guest,
						drv_guest,
						arg);
		if (result != (struct driver*)NULL)
			break;
	}
	return result;
}


struct driver* for_each_driver(drv_guest_func func,
			       void* arg,
			       const char* pluginpath)
{
	return for_each_path(visit_plugin, func, arg, pluginpath);
}


void for_each_plugin(plugin_guest_func plugin_guest,
		     void* arg,
		     const char* pluginpath)
{
	for_each_path(plugin_guest, NULL, arg, pluginpath);
}


/**
 * @brief Prints all drivers known to the system to the file given as argument.
 * @param file File to print to.
 */
void hw_print_drivers(FILE* file)
{
	char_array names;
	int i;

	names.size = 0;
	if (for_each_driver(add_hw_name, (void*)&names, NULL) != NULL) {
		fprintf(stderr, "Too many plugins (%d)\n", MAX_PLUGINS);
		return;
	}
	qsort(names.array, names.size, sizeof(char*), line_cmp);
	for (i = 0; i < names.size; i += 1) {
		fprintf(file, "%s\n", names.array[i]);
		free(names.array[i]);
	}
}


int hw_choose_driver(const char* name)
{
	struct driver* found;

	if (name == NULL) {
		memcpy(&drv, &drv_null, sizeof(struct driver));
		return 0;
	}
	if (strcasecmp(name, "dev/input") == 0)
		/* backwards compatibility */
		name = "devinput";
	found = for_each_driver(match_hw_name, (void*)name, NULL);
	if (found != (struct driver*)NULL) {
		memcpy(&drv, found, sizeof(struct driver));
		return 0;
	}
	return -1;
}

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <dirent.h>
#include <dlfcn.h>

#include "lirc/hardware.h"
#include "lirc/hw-types.h"
#include "lirc/lirc_options.h"

struct hardware hw;             // Set by hw_choose_driver(), the driver in use.

static void* last_plugin = NULL;


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


static int ends_with_so(const char *str)
// Return 0 if str ends with ".so".
{
    char *dot = strrchr(str, '.');

    if (NULL == dot) return 0;
    return strcmp(dot, ".so") == 0;
}


static struct hardware* print_hw_name(struct hardware* hw, void* file)
// hw_guest_func which prints name of *hw on file. Returns NULL.
{
	fprintf((FILE*)file, "\t%s\n", hw->name);
	return (struct hardware*)NULL;
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


static struct hardware* for_each_driver(hw_guest_func func, void* arg)
// Apply func(hw, arg) for all drivers found in all plugins.
{
	DIR* dir;
	struct dirent* ent;
	struct hardware* result = (struct hardware*) NULL;
	char* plugindir;
	char path[128];

	plugindir = ciniparser_getstring(lirc_options,
					 "lircd:plugindir",
					 getenv(PLUGINDIR_VAR));
	if (plugindir == NULL)
		plugindir = PLUGINDIR;
	if ((dir = opendir(plugindir)) == NULL){
		logprintf(LOG_ERR, "Cannot open plugindir %s", plugindir);
		return  (struct hardware*) NULL;
	}
	while ((ent = readdir(dir)) != NULL) {
		if (!ends_with_so(ent->d_name))
			continue;
		snprintf(path, sizeof(path),
			 "%s/%s", plugindir, ent->d_name);
		result = visit_plugin(path, func, arg);
		if (result != (struct hardware*) NULL)
			break;
	}
	closedir(dir);
	return result;
}


void hw_print_drivers(FILE* file)
// Print list of all hardware names (i. e., drivers) on file.
{
	for_each_driver(print_hw_name, (void*)file);
}


int hw_choose_driver(char* name)
// Search for driver, update global hw with driver data if found.
// Returns 0 if found and hw updated, else -1.
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
		return 0;
	}
	return -1;
}

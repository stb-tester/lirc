
/****************************************************************************
 ** config_flags.h ***********************************************************
 ****************************************************************************
 *
 * config_flags.h - Flags shared between config_file and dump_config.
 *
 */

#ifndef  _CONFIG_FLAGS_H
#define  _CONFIG_FLAGS_H

struct flaglist {
	char* name;
	int flag;
};

extern const struct flaglist all_flags[];

#endif

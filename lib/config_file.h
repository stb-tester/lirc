
/****************************************************************************
 ** config_file.h ***********************************************************
 ****************************************************************************
 *
 * config_file.h - parses the config file of lircd
 *
 * Copyright (C) 1998 Pablo d'Angelo (pablo@ag-trek.allgaeu.org)
 *
 */

#ifndef  _CONFIG_FILE_H
#define  _CONFIG_FILE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "lirc/ir_remote.h"



struct ir_remote* read_config(FILE* f, const char* name);
void free_config(struct ir_remote* remotes);

#ifdef	__cplusplus
}
#endif

#endif

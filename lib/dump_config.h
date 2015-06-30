/****************************************************************************
** dump_config.h ***********************************************************
****************************************************************************
*
* dump_config.h - dumps data structures into file
*
* Copyright (C) 1998 Pablo d'Angelo <pablo@ag-trek.allgaeu.org>
*
*/

/**
 * @file dump_config.h
 *
 * @brief Dumps data structures into file.
 * @author  Pablo d'Angelo
 * @ingroup private_api
 */

#ifndef _DUMP_CONFIG_H
#define  _DUMP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif



#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "ir_remote.h"

void fprint_comment(FILE* f,
		    const struct ir_remote* rem,
		    const char* commandline);

void fprint_flags(FILE* f, int flags);

void fprint_remotes(FILE* f,
		    const struct ir_remote* all,
		    const char* commandline);

void fprint_remote_gap(FILE* f, const struct ir_remote* rem);

void fprint_remote_head(FILE* f, const struct ir_remote* rem);

void fprint_remote_foot(FILE* f, const struct ir_remote* rem);

void fprint_remote_signal_head(FILE* f, const struct ir_remote* rem);

void fprint_remote_signal_foot(FILE* f, const struct ir_remote* rem);

void fprint_remote_signal(FILE* f,
			  const struct ir_remote* rem,
			  const struct ir_ncode* codes);

void fprint_remote_signals(FILE* f, const struct ir_remote* rem);

void fprint_remote(FILE* f,
		   const struct ir_remote* rem,
		   const char* commandline);

#ifdef __cplusplus
}
#endif

#endif

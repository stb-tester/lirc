/**
 *  @file drv_enum.c
 *  @author Alec Leamas
 *  @date August 2014
 *  @ense GPL2 or later
 *  @brief Implemenents drv_enum.h
 *  @ingroup driver_api
 */

#include <glob.h>

#include "drv_enum.h"

void drv_enum_free(glob_t* glob)
{
	int i;

	for (i = 0; i < glob->gl_pathc; i += 1)
		free(glob->gl_pathv[i]);
	free(glob->gl_pathv);
}

glob_t* drv_enum_glob(glob_t* globbuf, const char* pattern)
{
	glob_t buff;
	int i;

	buff.gl_offs = 128;
	buff.gl_pathc = 0;
	if (glob(pattern, 0, NULL, &buff) != 0)
		return globbuf;
	globbuf->gl_pathc = 0;
	globbuf->gl_pathv = (char**) calloc(64, sizeof(char*));
	for (i = 0; i < buff.gl_pathc; i += 1)
		globbuf->gl_pathv[i] = strdup(buff.gl_pathv[i]);
	globbuf->gl_pathc = buff.gl_pathc;
	globbuf->gl_offs = buff.gl_pathc;
	globfree(&buff);
	return globbuf;
}

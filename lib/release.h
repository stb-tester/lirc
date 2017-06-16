/****************************************************************************
** release.h ***************************************************************
****************************************************************************
* Copyright (C) 2007 Christoph Bartelmus <lirc@bartelmus.de>
*/

/**
 * @file release.h
 * @brief Automatic release event generation.
 * @author Christoph Bartelmus
 * @ingroup private_api
 */

#ifndef RELEASE_H
#define RELEASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ir_remote_types.h"

/**
 * Set up pending events for given button, including the
 * release_gap. Data is saved to be retrieved using get_release_data().
 */
void register_button_press(struct ir_remote* remote,
			   struct ir_ncode*  ncode,
			   ir_code           code,
			   int               reps);


/** Get data from saved from last call to register_button_press(). */
void get_release_data(const char** remote_name,
		      const char** button_name,
		      int*         reps);

/**
 *  Get time for last call to register_button_press() if defined, else a noop.
 */
void get_release_time(struct timeval* tv);


#ifdef __cplusplus
}
#endif

#endif /* RELEASE_H */

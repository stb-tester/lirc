/****************************************************************************
** lirc-utils.h ************************************************************
****************************************************************************
*
* Miscellanious utility functions.
*
*/

/**
 * @file lirc-utils.h
 * @author Alec Leamas
 * @brief Utilities
 * @ingroup private_api
 */

#ifndef _UTIL_H
#define _UTIL_H

#include "lirc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup private_api
 * @{
 */


/**
 *  Try to drop possible root privileges, returning new user or "" on error.
 *  @param set_some_uid Typically seteuid() or setuid()
 *  @note Uses environment SUDO_USER to determine user to switch to (if any).
 */
const char* drop_sudo_root(int (*set_some_uid)(uid_t));

/**
*   Default view part of drop_sudo_root. Invokes drop_sudo_root() and prints
*   status messagea on stdout.
*   @param set_some_uid Typically seteuid() or setuid()
*/
void drop_root_cli(int (*set_some_uid)(uid_t));


/** @} */
#ifdef __cplusplus
}
#endif

#endif

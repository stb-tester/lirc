#ifndef HEADER_LIB_CURL_POLL_H
#define HEADER_LIB_CURL_POLL_H
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2012, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

/**
 * @file curl_poll.h
 * @brief  Wrapper for poll(2) using select(2) when poll() is unavailable.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#else
#include <poll.h>
#endif

int curl_poll(struct pollfd ufds[], unsigned int nfds, int timeout_ms);

#ifdef __cplusplus
}
#endif


#endif /* HEADER_LIB_CURL_POLL_H */

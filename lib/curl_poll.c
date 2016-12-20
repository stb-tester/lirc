/***************************************************************************
*                                  _   _ ____  _
*  Project                     ___| | | |  _ \| |
*                             / __| | | | |_) | |
*                            | (__| |_| |  _ <| |___
*                             \___|\___/|_| \_\_____|
*
* Copyright (C) 1998 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
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
#define _XOPEN_SOURCE 700

#include "config.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if !defined(HAVE_SELECT) && !defined(HAVE_POLL_FINE)
#error "We can't compile without select() or poll() support."
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>

#include "lirc_log.h"
#include "curl_poll.h"


/* Convenience local macros */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static const logchannel_t logchannel = LOG_LIB;

/*
 * A wrapper around poll().  If poll() does not exist, then
 * select() is used instead.  An error is returned if select() is
 * being used and a file descriptor is too large for FD_SETSIZE.
 * A negative timeout value makes this function wait indefinitely,
 * unles no valid file descriptor is given, when this happens the
 * negative timeout is ignored and the function times out immediately.
 *
 * Return values:
 *   -1 = system call error or invalid fd. errno as set by poll()/select()
 *        or EINVAL if fd > FD_SETSIZE.
 *    0 = timeout
 *    N = number of structures with non zero revent fields
 */

#ifdef HAVE_POLL_FINE

int curl_poll(struct pollfd ufds[], unsigned int nfds, int timeout_ms)
{
	return poll(ufds, nfds, timeout_ms);
}

#else

static struct timeval curlx_tvnow(void)
{
	/*
	** gettimeofday() is not granted to be increased monotonically, due to
	** clock drifting and external source time synchronization it can jump
	** forward or backward in time.
	*/
	struct timeval now;

	(void)gettimeofday(&now, NULL);
	return now;
}


/*
 * Make sure that the first argument is the more recent time, as otherwise
 * we'll get a weird negative time-diff back...
 *
 * Returns: the time difference in number of milliseconds.
 */
long curlx_tvdiff(struct timeval newer, struct timeval older)
{
	return (newer.tv_sec - older.tv_sec) * 1000 +
	       (long)(newer.tv_usec - older.tv_usec) / 1000;
}


static int verify_sock(int s)
{
	if (s < 0 || s >= FD_SETSIZE) {
		errno = EINVAL;
		log_notice("curl_poll:  Invalid socket %d", s);
		return -1;
	}
	return s;
}


int curl_poll(struct pollfd ufds[], unsigned int nfds, int timeout_ms)
{
	struct timeval pending_tv;
	struct timeval* ptimeout;
	fd_set fds_read;
	fd_set fds_write;
	fd_set fds_err;
	int maxfd;

	struct timeval initial_tv = { 0, 0 };
	unsigned int i;
	int pending_ms = 0;
	int r;

	/* Avoid initial timestamp, avoid curlx_tvnow() call, when elapsed
	 * time in this function does not need to be measured. This happens
	 * when function is called with a zero timeout or a negative timeout
	 * value indicating a blocking call should be performed. */

	if (timeout_ms > 0) {
		pending_ms = timeout_ms;
		gettimeofday(&initial_tv, NULL);
	}

	FD_ZERO(&fds_read);
	FD_ZERO(&fds_write);
	FD_ZERO(&fds_err);
	maxfd = (int)-1;

	for (i = 0; i < nfds; i++) {
		ufds[i].revents = 0;
		if (ufds[i].fd == -1)
			continue;
		ufds[i].fd = verify_sock(ufds[i].fd);
		if (ufds[i].events & (POLLIN | POLLOUT | POLLPRI |
				      POLLRDNORM | POLLWRNORM | POLLRDBAND)) {
			if (ufds[i].fd > maxfd)
				maxfd = ufds[i].fd;
			if (ufds[i].events & (POLLRDNORM | POLLIN))
				FD_SET(ufds[i].fd, &fds_read);
			if (ufds[i].events & (POLLWRNORM | POLLOUT))
				FD_SET(ufds[i].fd, &fds_write);
			if (ufds[i].events & (POLLRDBAND | POLLPRI))
				FD_SET(ufds[i].fd, &fds_err);
		}
	}

	ptimeout = (timeout_ms < 0) ? NULL : &pending_tv;

	if (timeout_ms > 0) {
		pending_tv.tv_sec = pending_ms / 1000;
		pending_tv.tv_usec = (pending_ms % 1000) * 1000;
	} else if (!timeout_ms) {
		pending_tv.tv_sec = 0;
		pending_tv.tv_usec = 0;
	}
	r = select((int)maxfd + 1, &fds_read, &fds_write, &fds_err,
		   ptimeout);
	if (r < 0)
		return -1;
	if (r == 0)
		return 0;
	r = 0;
	for (i = 0; i < nfds; i++) {
		ufds[i].revents = 0;
		if (ufds[i].fd == -1)
			continue;
		if (FD_ISSET(ufds[i].fd, &fds_read))
			ufds[i].revents |= POLLIN;
		if (FD_ISSET(ufds[i].fd, &fds_write))
			ufds[i].revents |= POLLOUT;
		if (FD_ISSET(ufds[i].fd, &fds_err))
			ufds[i].revents |= POLLPRI;
		if (ufds[i].revents != 0)
			r++;
	}
	return r;
}

#endif  /* HAVE_POLL_FINE */

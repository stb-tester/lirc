/****************************************************************************
 ** lircd.c *****************************************************************
 ****************************************************************************
 *
 * lircd - LIRC Decoder Daemon
 *
 * Copyright (C) 1996,97 Ralph Metzler <rjkm@thp.uni-koeln.de>
 * Copyright (C) 1998,99 Christoph Bartelmus <lirc@bartelmus.de>
 *
 *  =======
 *  HISTORY
 *  =======
 *
 * 0.1:  03/27/96  decode SONY infra-red signals
 *                 create mousesystems mouse signals on pipe /dev/lircm
 *       04/07/96  send ir-codes to clients via socket (see irpty)
 *       05/16/96  now using ir_remotes for decoding
 *                 much easier now to describe new remotes
 *
 * 0.5:  09/02/98 finished (nearly) complete rewrite (Christoph)
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif


#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "lirc/lirc_log.h"

#ifndef USE_SYSLOG
#define HOSTNAME_LEN 128
char hostname[HOSTNAME_LEN + 1];
FILE *lf = NULL;
#endif

int debug = 0;

#ifdef USE_SYSLOG
const char *syslogident = "lircd-" VERSION;
#else
char *logfile = LOGFILE;
#endif

char progname[128] = {'?','\0'};
static int nodaemon = 0;


static int log_enabled = 1;

static const char* prio2text(int prio)
{
	switch (prio) {
		case LOG_DEBUG:		return "Debug";
		case LOG_NOTICE:	return "Notice";
		case LOG_INFO:		return "Info";
		case LOG_WARNING:	return "Warning";
		case LOG_ERR:		return "Error";
		case LIRC_TRACE:        return "Trace";
		case LIRC_PEEP:         return "Trace1";
		case LIRC_STALK:        return "Trace2";
		default:		return "(Bad prio)";
	}
}

void lirc_set_logfile(char* s)
{
#	ifndef USE_SYSLOG
	logfile = s;
#	endif
}

int lirc_log_open(const char* _progname, int _nodaemon, int _debug)
{
	strncpy(progname, _progname, sizeof(progname));
	nodaemon = _nodaemon;
	debug = _debug;

#ifdef USE_SYSLOG
#ifdef DAEMONIZE
	if (nodaemon) {
		openlog(syslogident, LOG_CONS | LOG_PID | LOG_PERROR, LIRC_SYSLOG);
	} else {
		openlog(syslogident, LOG_CONS | LOG_PID, LIRC_SYSLOG);
	}
#else
	openlog(syslogident, LOG_CONS | LOG_PID | LOG_PERROR, LIRC_SYSLOG);
#endif
#else
	lf = fopen(logfile, "a");
	if (lf == NULL) {
		fprintf(stderr, "%s: could not open logfile\n", progname);
		perror(progname);
		return 1;
	}
	gethostname(hostname, HOSTNAME_LEN);
#endif
	return 0;
}

int lirc_log_reopen(void)
{
#ifdef USE_SYSLOG
	/* we don't need to do anyting as this is syslogd's task */
#else
	struct stat s;

	logprintf(LOG_INFO, "closing logfile");
	if (-1 == fstat(fileno(lf), &s)) {
		perror("Invalid logfile!");
		return -1;
	}
	fclose(lf);
	lf = fopen(logfile, "a");
	if (lf == NULL) {
		/* can't print any error messagees */
		perror("Can't open logfile");
		return -1;
	}
	logprintf(LOG_INFO, "reopened logfile");
	if (-1 == fchmod(fileno(lf), s.st_mode)) {
		logprintf(LOG_WARNING, "could not set file permissions");
		logperror(LOG_WARNING, NULL);
	}
#endif
	return 0;
}

int lirc_log_setlevel(const char* s)
{
	long level = LONG_MAX;

	if (s == NULL)
		s  = getenv("LIRC_LOGLEVEL");
	level = strtol(s, NULL, 10);
	if (level > INT_MAX || level < INT_MIN) {
		level = DEFAULT_LOGLEVEL;
		return 0;
	};
	debug = (int) level;
	return 1;
}


void log_enable(int enabled)
{
	log_enabled = enabled;
}


void logprintf(int prio, const char *format_str, ...)
{
	int save_errno = errno;
	va_list ap;

	if (!log_enabled)
		return;

	if (nodaemon && prio <= debug) {
		fprintf(stderr, "%s: %s", progname, prio2text(prio));
		va_start(ap, format_str);
		vfprintf(stderr, format_str, ap);
		va_end(ap);
		fputc('\n', stderr);
		fflush(stderr);
	}
#ifdef USE_SYSLOG
	va_start(ap, format_str);
	vsyslog(prio, format_str, ap);
	va_end(ap);
#else
	if (lf && prio <= debug) {
		time_t current;
		char *currents;

		current = time(&current);
		currents = ctime(&current);

		fprintf(lf, "%15.15s %s %s: ",
			currents + 4, hostname, progname);
		fprintf(lf, prio2text(prio));
		va_start(ap, format_str);
		vfprintf(lf, format_str, ap);
		va_end(ap);
		fputc('\n', lf);
		fflush(lf);
	}
#endif
	errno = save_errno;
}

void logperror(int prio, const char *s)
{
	if (!log_enabled)
		return;

#ifdef USE_SYSLOG
	if ((s) != NULL)
		syslog(prio, "%s: %m\n", (char *)s);
	else
		syslog(prio, "%m\n");
#else
	if (s != NULL) {
		logprintf(prio, "%s: %s", s, strerror(errno));
	} else {
		logprintf(prio, "%s", strerror(errno));
	}
#endif
}



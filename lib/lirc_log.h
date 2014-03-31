
/****************************************************************************
 ** lirc_log.h **************************************************************
 ****************************************************************************
 *
 */

#ifndef _LIRC_LOG_H
#define _LIRC_LOG_H

#include <syslog.h>
#include <sys/time.h>
#include <stdio.h>

/*
 * The maximum level logged. Either one of the syslog levels LOG_CRIT..
 * LOG_DEBUG, or one of LIRC_TRACE=8, LIRC_PEEP=9, LIRC_STALK=10.
 */
extern int debug;

#define LIRC_TRACE 8
#define LIRC_PEEP  9
#define LIRC_STALK 10

/* Set by lirc_log_open, convenience copy for clients. */
extern char progname[128];

#define DEFAULT_LOGLEVEL  3

#define logmax(l) (l > LOG_DEBUG ? LOG_DEBUG : l )

/*
 *  Debug tools. Accepts level 1..3 which are mapped to
 *  LIRC_TRACE..LIRC_STALK.
 */
#define LOGPRINTF(level,fmt,args...) \
	if (level + 7 <= debug ) logprintf(logmax(level + 7), fmt, ## args )

#define LOGPERROR(level,s) \
	if (level + 7 <= debug ) logperror(logmax(level + 7), s)

/*
 * Set the level using a string argument. If the level is NULL or
 * the string cant be parsed, use enviroemt variable LIRC_LOGLEVEL,
 * defaulting to DEFAULT_LOGLEVEL. Returns 0 if ok, 1 on errors.
 */
int lirc_log_setlevel(const char* level);

/*
 * Check if a given, standard loglevel should be printed.
 */
#define lirc_log_is_enabled_for(level) (level <= debug)

void logprintf(int prio, const char *format_str, ...);
void logperror(int prio, const char *s);
int lirc_log_reopen(void);
int lirc_log_open(const char* progname, int _nodaemon, int _debug);
void log_enable(int enabled);
void lirc_set_logfile(char* s);


#endif /* _LIRC_LOG_H */

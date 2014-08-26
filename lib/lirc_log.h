
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

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * The defined loglevels. LIRC_TRACE..LIRC_STALK is mapped to LIRC_DEBUG in
 * outputted messages, but generates more messages than DEBUG.
 */

typedef enum {
	LIRC_STALK 	= 10,
	LIRC_PEEP 	= 9,
	LIRC_TRACE 	= 8,
	LIRC_DEBUG 	= LOG_DEBUG,
	LIRC_INFO 	= LOG_INFO,
	LIRC_NOTICE	= LOG_NOTICE,
	LIRC_WARNING 	= LOG_WARNING,
	LIRC_ERROR 	= LOG_ERR,
	LIRC_NOLOG	= 0,
	LIRC_BADLEVEL   = -1
} loglevel_t;

#define LIRC_MAX_LOGLEVEL LIRC_STALK
#define LIRC_MIN_LOGLEVEL LIRC_ERROR

extern loglevel_t loglevel;

/* Set by lirc_log_open, convenience copy for clients. */
extern char progname[128];

#define DEFAULT_LOGLEVEL LIRC_INFO

#ifdef __cplusplus
#define logmax(l) (l > LIRC_DEBUG ? LIRC_DEBUG : static_cast<loglevel_t>(l) )
#else
#define logmax(l) (l > LIRC_DEBUG ? LIRC_DEBUG : l )
#endif

/*
 *  Debug tools. Accepts level 1..3 which are mapped to
 *  LIRC_TRACE..LIRC_STALK.
 */
#define LOGPRINTF(level,fmt,args...) \
	if (level + 7 <= loglevel ) logprintf(logmax(level + 7), fmt, ## args)

#define LOGPERROR(level,s) \
	if (level + 7 <= loglevel ) logperror(logmax(level + 7), s)


/**
 * Convert a string, either a number or 'info', 'peep', error etc.
 * to a loglevel.
 */
loglevel_t string2loglevel(const char* level);

/** Set the level Returns 1 if ok, 0 on errors. */
int lirc_log_setlevel(loglevel_t level);

/** Get the default level, from environment or hardcoded. */
loglevel_t lirc_log_defaultlevel(void);

/** Check if a given, standard loglevel should be printed.  */
#define lirc_log_is_enabled_for(level) (level <= loglevel)

/* Check if log is set up to use syslog or not. */
int lirc_log_use_syslog();

void logprintf(loglevel_t prio, const char *format_str, ...);
void logperror(loglevel_t prio, const char *s);
int lirc_log_reopen(void);
int lirc_log_open(const char* progname, int _nodaemon, loglevel_t level);
int lirc_log_close();

/*
 * Set logfile. Either a regular path or the string 'syslog'; the latter
 * does indeed use syslog(1) instead.
 * */
void lirc_log_set_file(const char* s);
void hexdump(char* prefix, unsigned char*  buf, int len);

#ifdef __cplusplus
}
#endif

#endif /* _LIRC_LOG_H */

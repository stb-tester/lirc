
/****************************************************************************
 ** lirc_log.h **************************************************************
 ****************************************************************************
 *
 */

/**
 * @file lirc_log.h
 * @brief Logging functionality.
 * @ingroup private_api
 * @ingroup driver_api
 */


#ifndef _LIRC_LOG_H
#define _LIRC_LOG_H

#include <syslog.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * @addtogroup driver_api
 */


/**
 * The defined loglevels. LIRC_TRACE..LIRC_TRACE2 is mapped to LIRC_DEBUG in
 * outputted messages, but generates more messages than DEBUG.
 */
typedef enum {
	LIRC_TRACE2	= 10,
	LIRC_TRACE1	= 9,
	LIRC_TRACE 	= 8,
	LIRC_DEBUG 	= LOG_DEBUG,
	LIRC_INFO 	= LOG_INFO,
	LIRC_NOTICE	= LOG_NOTICE,
	LIRC_WARNING 	= LOG_WARNING,
	LIRC_ERROR 	= LOG_ERR,
	LIRC_NOLOG	= 0,
	LIRC_BADLEVEL   = -1
} loglevel_t;

/** Max loglevel (for validation). */
#define LIRC_MAX_LOGLEVEL LIRC_TRACE2

/** Mix loglevel (for validation). */
#define LIRC_MIN_LOGLEVEL LIRC_ERROR

/** The actual loglevel. Should not be changed directly by external code.*/
extern loglevel_t loglevel;

/* Set by lirc_log_open, convenience copy for clients. */
extern char progname[128];

/** Default loglevel (last resort). */
#define DEFAULT_LOGLEVEL LIRC_INFO

/** Max level logged in actual logfile. */
#ifdef __cplusplus
#define logmax(l) (l > LIRC_DEBUG ? LIRC_DEBUG : static_cast<loglevel_t>(l) )
#else
#define logmax(l) (l > LIRC_DEBUG ? LIRC_DEBUG : l )
#endif

/**
 *  Compatibility log message stuff.. Accepts level 1..3 which are mapped to
 *  LIRC_TRACE..LIRC_TRACE2.
 */
#define LOGPRINTF(level,fmt,args...) \
	if (level + 7 <= loglevel ) logprintf(logmax(level + 7), fmt, ## args)

/**
 *  Compatibility perror(3) wrapper Accepts level 1..3 which are mapped to
 *  LIRC_TRACE..LIRC_TRACE2.
 */
#define LOGPERROR(level,s) \
	if (level + 7 <= loglevel ) logperror(logmax(level + 7), s)



/**
 * Convert a string, either a number or 'info', 'trace1', error etc.
 * to a loglevel.
 */
loglevel_t string2loglevel(const char* level);

/** Set the level. Returns 1 if ok, 0 on errors. */
int lirc_log_setlevel(loglevel_t level);

/** Get the default level, from environment or hardcoded. */
loglevel_t lirc_log_defaultlevel(void);

/** Check if a given, standard loglevel should be printed.  */
#define lirc_log_is_enabled_for(level) (level <= loglevel)

/** Check if log is set up to use syslog or not. */
int lirc_log_use_syslog();

/**
 * Write a message to log.
 *
 * @param prio Level of message
 * @param format_str,... printf-style string.
 */
void logprintf(loglevel_t prio, const char *format_str, ...);

/** Log current kernel error with a given level. */
void logperror(loglevel_t prio, const char *format, ...);
int lirc_log_reopen(void);

/**
 * Open the log for upcoming logging
 *
 * @param progname Name of application, made available in global progname
 * @param nodaemon If true, program runs in foreground and logging is on also
 *     on stdout.
 * @param level The lowest level of messages to actually be logged.
 * @return 0 if OK, else positive error code.
 */
int lirc_log_open(const char* progname, int _nodaemon, loglevel_t level);

/** Close the log previosly opened with lirc_log_open(). */
int lirc_log_close();

/**
 * Set logfile. Either a regular path or the string 'syslog'; the latter
 * does indeed use syslog(1) instead. Must be called before lirc_log_open().
 */
void lirc_log_set_file(const char* s);

/**
 * Retrieve a client path for logging according to freedesktop specs.
 *
 * @param basename  Basename for the logfile.
 * @param buff Buffer to store result in.
 * @param size Size of buffer
 * @return 0 if OK, otherwise -1
 */
int lirc_log_get_clientlog(const char* basename, char* buffer, ssize_t size);

/** Print prefix + a hex dump of len bytes starting at  *buf. */
void hexdump(char* prefix, unsigned char*  buf, int len);

/** Helper macro for STR().*/
#define STRINGIFY(x) #x

/** Return x in (double) quotes. */
#define STR(x) STRINGIFY(x)

/** Wrapper for write(2) which logs errors. */
#define chk_write(fd, buf, count) \
	do_chk_write(fd, buf, count, STR(__FILE__) ":" STR(__LINE__))


/** Wrapper for read(2) which logs errors. */
#define chk_read(fd, buf, count) \
	do_chk_read(fd, buf, count, STR(__FILE__) ":" STR(__LINE__))


/** Implement the chk_write() macro. */
static inline void
do_chk_write(int fd, const void *buf, size_t count, const char* msg)
{
	if (write(fd, buf, count) == -1) {
		logperror(LIRC_WARNING, msg);
	}
}


/** Implement the chk_read() macro. */
static inline void
do_chk_read(int fd, void *buf, size_t count, const char* msg) 
{
	if (read(fd, buf, count) == -1) {
		logperror(LIRC_WARNING, msg);
	}
}



/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _LIRC_LOG_H */

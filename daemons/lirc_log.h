
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

#ifdef DEBUG
#define LOGPRINTF(level,fmt,args...)	\
  if(level<=debug) logprintf(LOG_DEBUG,fmt, ## args )
#define LOGPERROR(level,s) \
  if(level<=debug) logperror(LOG_DEBUG,s)
#else
#define LOGPRINTF(level,fmt,args...)	\
  do {} while(0)
#define LOGPERROR(level,s) \
  do {} while(0)
#endif

FILE* lf;

void logprintf(int prio, const char *format_str, ...);
void logperror(int prio, const char *s);
int lirc_log_reopen(void);
int lirc_log_open(int nodaemon_arg, char* progname_arg, int daemonized_arg);
void log_enable(int enabled);
void lirc_set_logfile(char* s);


#endif /* _LIRC_LOG_H */

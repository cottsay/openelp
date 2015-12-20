#ifndef _log_syslog_h
#define _log_syslog_h

#ifdef OPENELP_USE_SYSLOG
#include <syslog.h>
#else
#define LOG_CONS 0
#define LOG_NDELAY 0
#define LOG_DAEMON 0
#define LOG_CRIT 0
#define LOG_ERR 0
#define LOG_WARNING 0
#define LOG_INFO 0
#define LOG_DEBUG 0
#define closelog()
#define openlog(ident, option, facility)
#define vsyslog(facility_priority, format, arglist)
#endif

#endif /* _log_syslog_h */

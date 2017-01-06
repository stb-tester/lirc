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

/**
 * @file lircd.c
 * This file implements the main daemon lircd.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/file.h>
#include <pwd.h>
#include <poll.h>

#if defined(__linux__)
#include <linux/input.h>
#include <linux/uinput.h>
#include "lirc/input_map.h"
#endif

#ifdef HAVE_SYSTEMD
#include "systemd/sd-daemon.h"
#endif

#if defined __APPLE__ || defined __FreeBSD__
#include <sys/ioctl.h>
#endif

#include <string>
#include <set>

#include "lirc_private.h"

#ifdef HAVE_INT_GETGROUPLIST_GROUPS
#define lirc_gid int
#else
#define lirc_gid gid_t
#endif

#ifdef DARWIN
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC SYSTEM_CLOCK
int clock_gettime(int clk_id, struct timespec *t){
	static mach_timebase_info_data_t timebase = {0};
	uint64_t time;

	if (timebase.numer == 0)
		mach_timebase_info(&timebase);
	time = mach_absolute_time();
	t->tv_nsec = ((double) time *
		    (double) timebase.numer)/((double) timebase.denom);
	t->tv_sec = ((double)time *
		   (double)timebase.numer)/((double)timebase.denom * 1e9);
	return 0;
}
#endif


/****************************************************************************
** lircd.h *****************************************************************
****************************************************************************
*
*/

#define DEBUG_HELP "Bad debug level: \"%s\"\n\n" \
	"Level could be ERROR, WARNING, NOTICE, INFO, DEBUG, TRACE, TRACE1,\n" \
	" TRACE2 or a number in the range 3..10.\n"

#ifndef PACKET_SIZE
#define PACKET_SIZE 256
#endif
#define WHITE_SPACE " \t"

static const logchannel_t logchannel = LOG_APP;

/** How long we sleep while waiting for busy write sockets. */
static const int WRITE_SLEEP_US = 20000;

/** How many times we retry busy write sockets. */
static const int WRITE_RETRIES = 50;

struct peer_connection {
	char*		host;
	unsigned short	port;
	struct timeval	reconnect;
	int		connection_failure;
	int		socket;
};


static const char* const help =
	"Usage: lircd [options] <config-file>\n"
	"\t -h --help\t\t\tDisplay this message\n"
	"\t -v --version\t\t\tDisplay version\n"
	"\t -O --options-file\t\tOptions file\n"
	"\t -n --nodaemon\t\t\tDon't fork to background\n"
	"\t -p --permission=mode\t\tFile permissions for " LIRCD "\n"
	"\t -H --driver=driver\t\tUse given driver (-H help lists drivers)\n"
	"\t -d --device=device\t\tRead from given device\n"
	"\t -U --plugindir=dir\t\tDir where drivers are loaded from\n"
	"\t -l --listen[=[address:]port]\tListen for network connections\n"
	"\t -c --connect=host[:port]\tConnect to remote lircd server\n"
	"\t -o --output=socket\t\tOutput socket filename\n"
	"\t -P --pidfile=file\t\tDaemon pid file\n"
	"\t -L --logfile=file\t\tLog file path (default: use syslog)'\n"
	"\t -D[level] --loglevel[=level]\t'info', 'warning', 'notice', etc., or 3..10.\n"
	"\t -r --release[=suffix]\t\tAuto-generate release events\n"
	"\t -a --allow-simulate\t\tAccept SIMULATE command\n"
	"\t -Y --dynamic-codes\t\tEnable dynamic code generation\n"
	"\t -A --driver-options=key:value[|key:value...]\n"
	"\t\t\t\t\tSet driver options\n"
#       if defined(__linux__)
	"\t -u --uinput\t\t\tgenerate Linux input events\n"
#       endif
	"\t -e --effective-user=uid\t\tRun as uid after init as root\n"
	"\t -R --repeat-max=limit\t\tallow at most this many repeats\n";


static const struct option lircd_options[] = {
	{ "help",	    no_argument,       NULL, 'h' },
	{ "version",	    no_argument,       NULL, 'v' },
	{ "nodaemon",	    no_argument,       NULL, 'n' },
	{ "options-file",   required_argument, NULL, 'O' },
	{ "permission",	    required_argument, NULL, 'p' },
	{ "driver",	    required_argument, NULL, 'H' },
	{ "device",	    required_argument, NULL, 'd' },
	{ "listen",	    optional_argument, NULL, 'l' },
	{ "connect",	    required_argument, NULL, 'c' },
	{ "output",	    required_argument, NULL, 'o' },
	{ "pidfile",	    required_argument, NULL, 'P' },
	{ "plugindir",	    required_argument, NULL, 'U' },
	{ "logfile",	    required_argument, NULL, 'L' },
	{ "debug",	    optional_argument, NULL, 'D' }, // compatibility
	{ "loglevel",	    optional_argument, NULL, 'D' },
	{ "release",	    optional_argument, NULL, 'r' },
	{ "allow-simulate", no_argument,       NULL, 'a' },
	{ "dynamic-codes",  no_argument,       NULL, 'Y' },
	{ "driver-options", required_argument, NULL, 'A' },
	{ "effective-user", required_argument, NULL, 'e' },
#        if defined(__linux__)
	{ "uinput",	    no_argument,       NULL, 'u' },
#        endif
	{ "repeat-max",	    required_argument, NULL, 'R' },
	{ 0,		    0,		       0,    0	 }
};


/* Forwards referenced in directive definition below. */

static int list(int fd, char* message, char* arguments);
static int set_transmitters(int fd, char* message, char* arguments);
static int set_inputlog(int fd, char* message, char* arguments);
static int simulate(int fd, char* message, char* arguments);
static int send_once(int fd, char* message, char* arguments);
static int drv_option(int fd, char* message, char* arguments);
static int send_start(int fd, char* message, char* arguments);
static int send_stop(int fd, char* message, char* arguments);
static int send_core(int fd, char* message, char* arguments, int once);
static int version(int fd, char* message, char* arguments);

struct protocol_directive {
	const char* name;
	int (*function)(int fd, char* message, char* arguments);
};


#ifndef timersub
#define timersub(a, b, result)                                            \
	do {                                                                    \
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                         \
		(result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                      \
		if ((result)->tv_usec < 0) {                                          \
			--(result)->tv_sec;                                                 \
			(result)->tv_usec += 1000000;                                       \
		}                                                                     \
	} while (0)
#endif


static struct ir_remote* remotes;
static struct ir_remote* free_remotes = NULL;

static int repeat_fd = -1;
static char* repeat_message = NULL;
static __u32 repeat_max = REPEAT_MAX_DEFAULT;

static const char* configfile = NULL;
static FILE* pidf;
static const char* pidfile = PIDFILE;
static const char* lircdfile = LIRCD;

static const struct protocol_directive directives[] = {
	{ "LIST",	      list	       },
	{ "SEND_ONCE",	      send_once	       },
	{ "SEND_START",	      send_start       },
	{ "SEND_STOP",	      send_stop	       },
	{ "SET_INPUTLOG",     set_inputlog     },
	{ "DRV_OPTION",	      drv_option       },
	{ "VERSION",	      version	       },
	{ "SET_TRANSMITTERS", set_transmitters },
	{ "SIMULATE",	      simulate	       },
	{ NULL,		      NULL	       }
	/*
	 * {"DEBUG",debug},
	 * {"DEBUG_LEVEL",debug_level},
	 */
};

enum protocol_string_num {
	P_BEGIN = 0,
	P_DATA,
	P_END,
	P_ERROR,
	P_SUCCESS,
	P_SIGHUP
};

static const char* const protocol_string[] = {
	"BEGIN\n",
	"DATA\n",
	"END\n",
	"ERROR\n",
	"SUCCESS\n",
	"SIGHUP\n"
};

/* Used to be depending on FD_SETSIZE, but using poll() it's now arbitrary. */
static const int MAX_PEERS  = 256;
static const int MAX_CLIENTS = 256;

static int sockfd, sockinet;
static int do_shutdown;

static int uinputfd = -1;
static int clis[MAX_CLIENTS];

static int nodaemon = 0;
static loglevel_t loglevel_opt = LIRC_NOLOG;

#define CT_LOCAL  1
#define CT_REMOTE 2

static int cli_type[MAX_CLIENTS];
static int clin = 0; /* Number of clients */

static int listen_tcpip = 0;
static unsigned short int port = LIRC_INET_PORT;
static struct in_addr address;

static struct peer_connection* peers[MAX_PEERS];
static int peern = 0;

static int daemonized = 0;
static int allow_simulate = 0;
static int userelease = 0;
static int useuinput = 0;

static sig_atomic_t term = 0, hup = 0, alrm = 0;
static int termsig;

static __u32 setup_min_freq = 0, setup_max_freq = 0;
static lirc_t setup_max_gap = 0;
static lirc_t setup_min_pulse = 0, setup_min_space = 0;
static lirc_t setup_max_pulse = 0, setup_max_space = 0;

/* Use already opened hardware? */
int use_hw(void)
{
	return clin > 0 || (useuinput && uinputfd != -1) || repeat_remote != NULL;
}

/* set_transmitters only supports 32 bit int */
#define MAX_TX (CHAR_BIT * sizeof(__u32))

int max(int a, int b)
{
	return a > b ? a : b;
}

/* cut'n'paste from fileutils-3.16: */

#define isodigit(c) ((c) >= '0' && (c) <= '7')

/* Return a positive integer containing the value of the ASCII
* octal number S.  If S is not an octal number, return -1.  */

static int oatoi(const char* s)
{
	register int i;

	if (*s == 0)
		return -1;
	for (i = 0; isodigit(*s); ++s)
		i = i * 8 + *s - '0';
	if (*s)
		return -1;
	return i;
}

/* A safer write(), since sockets might not write all but only some of the
 * bytes requested */
int write_socket(int fd, const char* buf, int len)
{
	int done, todo = len;
	int retries = WRITE_RETRIES;

	while (todo) {
		done = write(fd, buf, todo);
		if (done <= 0) {
			log_perror_debug("Error in write_socket");
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				retries -= 1;
				if (retries <= 0)
					return done;
				usleep(WRITE_SLEEP_US);
				continue;
			} else {
				return done;
			}
		}
		buf += done;
		todo -= done;
	}
	return len;
}

int write_socket_len(int fd, const char* buf)
{
	int len;

	len = strlen(buf);
	if (write_socket(fd, buf, len) < len)
		return 0;
	return 1;
}


int read_timeout(int fd, char* buf, int len, int timeout_us)
{
	int ret, n;
	struct pollfd  pfd = {fd, POLLIN, 0}; // fd, events, revents
	int timeout = timeout_us > 0 ? timeout_us/1000 : -1;


	/* CAVEAT: (from libc documentation)
	 * Any signal will cause `select' to return immediately.  So if your
	 * program uses signals, you can't rely on `select' to keep waiting
	 * for the full time specified.  If you want to be sure of waiting
	 * for a particular amount of time, you must check for `EINTR' and
	 * repeat the `select' with a newly calculated timeout based on the
	 * current time.  See the example below.
	 *
	 * The timeout is not recalculated here although it should, we keep
	 * waiting as long as there are EINTR.
	 */
	do
		ret = curl_poll(&pfd, 1, timeout);
	while (ret == -1 && errno == EINTR);
	if (ret == -1) {
		log_perror_err("read_timeout: curl_poll() failed");
		return -1;
	};
	if (ret == 0)
		return 0;       /* timeout */
	n = read(fd, buf, len);
	if (n == -1) {
		log_perror_err("read_timeout: read() failed");
		return -1;
	}
	return n;
}


static int setup_frequency(void)
{
	__u32 freq;

	if (!(curr_driver->features & LIRC_CAN_SET_REC_CARRIER))
		return 1;
	if (setup_min_freq == 0 || setup_max_freq == 0) {
		setup_min_freq = DEFAULT_FREQ;
		setup_max_freq = DEFAULT_FREQ;
	}
	if (curr_driver->features & LIRC_CAN_SET_REC_CARRIER_RANGE && setup_min_freq != setup_max_freq) {
		if (curr_driver->drvctl_func(LIRC_SET_REC_CARRIER_RANGE, &setup_min_freq) == -1) {
			log_error("could not set receive carrier");
			log_perror_err(__func__);
			return 0;
		}
		freq = setup_max_freq;
	} else {
		freq = (setup_min_freq + setup_max_freq) / 2;
	}
	if (curr_driver->drvctl_func(LIRC_SET_REC_CARRIER, &freq) == -1) {
		log_error("could not set receive carrier");
		log_perror_err(__func__);
		return 0;
	}
	return 1;
}


static int setup_timeout(void)
{
	lirc_t val, min_timeout, max_timeout;
	__u32 enable = 1;

	if (!(curr_driver->features & LIRC_CAN_SET_REC_TIMEOUT))
		return 1;

	if (setup_max_space == 0)
		return 1;
	if (curr_driver->drvctl_func(LIRC_GET_MIN_TIMEOUT, &min_timeout) == -1
	    || curr_driver->drvctl_func(LIRC_GET_MAX_TIMEOUT, &max_timeout) == -1)
		return 0;
	if (setup_max_gap >= min_timeout && setup_max_gap <= max_timeout) {
		/* may help to detect end of signal faster */
		val = setup_max_gap;
	} else {
		/* keep timeout to a minimum */
		val = setup_max_space + 1;
		if (val < min_timeout)
			val = min_timeout;
		else if (val > max_timeout)
			/* maximum timeout smaller than maximum possible
			 * space, hmm */
			val = max_timeout;
	}

	if (curr_driver->drvctl_func(LIRC_SET_REC_TIMEOUT, &val) == -1) {
		log_error("could not set timeout");
		log_perror_err(__func__);
		return 0;
	}
	curr_driver->drvctl_func(LIRC_SET_REC_TIMEOUT_REPORTS, &enable);
	return 1;
}


static int setup_hardware(void)
{
	int ret = 1;

	if (curr_driver->fd != -1 && curr_driver->drvctl_func) {
		if ((curr_driver->features & LIRC_CAN_SET_REC_CARRIER)
		    || (curr_driver->features & LIRC_CAN_SET_REC_TIMEOUT)
		    || (curr_driver->features & LIRC_CAN_SET_REC_FILTER)) {
				ret = setup_frequency() && setup_timeout();
		}
	}
	return ret;
}

static void check_config_duplicates(const struct ir_remote* head)
{
	std::set<std::string> names;
	const struct ir_remote* ir;
	const char* const errmsg =
		"Duplicate remotes \"%s\" found, problems ahead";

	for (ir = head; ir != NULL; ir = ir->next) {
		std::string name(ir->name);
		if (names.count(name) == 1)
			log_warn(errmsg, name.c_str())
		else
			names.insert(name);
	}
}


void config(void)
{
	FILE* fd;
	struct ir_remote* config_remotes;
	const char* filename = configfile;

	if (filename == NULL)
		filename = LIRCDCFGFILE;

	if (free_remotes != NULL) {
		log_error("cannot read config file");
		log_error("old config is still in use");
		return;
	}
	fd = fopen(filename, "r");
	if (fd == NULL && errno == ENOENT && configfile == NULL) {
		/* try old lircd.conf location */
		int save_errno = errno;

		fd = fopen(LIRCDOLDCFGFILE, "r");
		if (fd != NULL)
			filename = LIRCDOLDCFGFILE;
		else
			errno = save_errno;
	}
	if (fd == NULL) {
		log_perror_err("could not open config file '%s'", filename);
		return;
	}
	configfile = filename;
	config_remotes = read_config(fd, configfile);
	check_config_duplicates(config_remotes);
	fclose(fd);
	if (config_remotes == (void*)-1) {
		log_error("reading of config file failed");
	} else {
		log_trace("config file read");
		if (config_remotes == NULL) {
			log_warn("config file %s contains no valid remote control definition",
				  filename);
		}
		/* I cannot free the data structure
		 * as they could still be in use */
		free_remotes = remotes;
		remotes = config_remotes;

		get_frequency_range(remotes, &setup_min_freq, &setup_max_freq);
		get_filter_parameters(remotes, &setup_max_gap, &setup_min_pulse, &setup_min_space, &setup_max_pulse,
				      &setup_max_space);

		setup_hardware();
	}
}


void remove_client(int fd)
{
	int i;

	for (i = 0; i < clin; i++) {
		if (clis[i] == fd) {
			shutdown(clis[i], 2);
			close(clis[i]);
			log_info("removed client");

			clin--;
			if (!useuinput && !use_hw() && curr_driver->deinit_func)
				curr_driver->deinit_func();
			for (; i < clin; i++)
				clis[i] = clis[i + 1];
			return;
		}
	}
	log_trace("internal error in remove_client: no such fd");
}


void sigterm(int sig)
{
	/* all signals are blocked now */
	if (term)
		return;
	term = 1;
	termsig = sig;
}

void dosigterm(int sig)
{
	int i;

	signal(SIGALRM, SIG_IGN);
	log_notice("caught signal");

	if (free_remotes != NULL)
		free_config(free_remotes);
	free_config(remotes);
	repeat_remote = NULL;
	for (i = 0; i < clin; i++) {
		shutdown(clis[i], 2);
		close(clis[i]);
	}
	;
	if (do_shutdown)
		shutdown(sockfd, 2);
	close(sockfd);

#if defined(__linux__)
	if (uinputfd != -1) {
		ioctl(uinputfd, UI_DEV_DESTROY);
		close(uinputfd);
		uinputfd = -1;
	}
#endif
	if (listen_tcpip) {
		shutdown(sockinet, 2);
		close(sockinet);
	}
	fclose(pidf);
	(void)unlink(pidfile);
	if (curr_driver->close_func)
		curr_driver->close_func();
	if (use_hw() && curr_driver->deinit_func)
		curr_driver->deinit_func();
	if (curr_driver->close_func)
		curr_driver->close_func();
	lirc_log_close();
	signal(sig, SIG_DFL);
	if (sig == SIGUSR1)
		exit(0);
	raise(sig);
}

void sighup(int sig)
{
	hup = 1;
}

void dosighup(int sig)
{
	int i;

	/* reopen logfile first */
	if (lirc_log_reopen() != 0) {
		/* can't print any error messagees */
		dosigterm(SIGTERM);
	}

	config();

	for (i = 0; i < clin; i++) {
		if (!
		    (write_socket_len(clis[i], protocol_string[P_BEGIN])
		     && write_socket_len(clis[i], protocol_string[P_SIGHUP])
		     && write_socket_len(clis[i], protocol_string[P_END]))) {
			remove_client(clis[i]);
			i--;
		}
	}
	/* restart all connection timers */
	for (i = 0; i < peern; i++) {
		if (peers[i]->socket == -1) {
			gettimeofday(&peers[i]->reconnect, NULL);
			peers[i]->connection_failure = 0;
		}
	}
}

int setup_uinputfd(const char* name)
{
#if defined(__linux__)
	int fd;
	int key;
	struct uinput_user_dev dev;

	fd = open("/dev/input/uinput", O_RDWR);
	if (fd == -1) {
		fd = open("/dev/uinput", O_RDWR);
		if (fd == -1) {
			fd = open("/dev/misc/uinput", O_RDWR);
			if (fd == -1) {
				perrorf("could not open %s", "uinput");
				return -1;
			}
		}
	}
	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, name, sizeof(dev.name));
	dev.name[sizeof(dev.name) - 1] = 0;
	if (write(fd, &dev, sizeof(dev)) != sizeof(dev) || ioctl(fd, UI_SET_EVBIT, EV_KEY) != 0
	    || ioctl(fd, UI_SET_EVBIT, EV_REP) != 0)
		goto setup_error;

	for (key = KEY_RESERVED; key <= KEY_UNKNOWN; key++)
		if (ioctl(fd, UI_SET_KEYBIT, key) != 0)
			goto setup_error;

	if (ioctl(fd, UI_DEV_CREATE) != 0)
		goto setup_error;
	return fd;

setup_error:
	perrorf("could not setup %s", "uinput");
	close(fd);
#endif
	return -1;
}

void nolinger(int sock)
{
	static struct linger linger = { 0, 0 };
	int lsize = sizeof(struct linger);

	setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&linger, lsize);
}


void drop_privileges(void)
{
	const char* user;
	struct passwd* pw;
	lirc_gid groups[32];
	int group_cnt = sizeof(groups)/sizeof(gid_t);
	char groupnames[256] = {0};
	char buff[12];
	int r;
	int i;

	if (getuid() != 0)
		return;
	user = options_getstring("lircd:effective-user");
	if (user == NULL || strlen(user) == 0) {
		log_warn("Running as root");
		return;
	}
	pw = getpwnam(user);
	if (pw == NULL) {
		log_perror_warn("Illegal effective uid: %s", user);
		return;
	}
	r = getgrouplist(user, pw->pw_gid, groups, &group_cnt);
	if (r == -1) {
		log_perror_warn("Cannot get supplementary groups");
		return;
	}
	r = setgroups(group_cnt, (const gid_t*) groups);
	if (r == -1) {
		log_perror_warn("Cannot set supplementary groups");
		return;
	}
	r = setgid(pw->pw_gid);
	if (r == -1) {
		log_perror_warn("Cannot set GID");
		return;
	}
	r = setuid(pw->pw_uid);
	if (r == -1) {
		log_perror_warn("Cannot change UID");
		return;
	}
	log_notice("Running as user %s", user);
	for (i = 0; i < group_cnt; i += 1) {
		snprintf(buff, 5, " %d", groups[i]);
		strcat(groupnames, buff);
	}
	log_debug("Groups: [%d]:%s", pw->pw_gid, groupnames);
}


void add_client(int sock)
{
	int fd;
	socklen_t clilen;
	struct sockaddr client_addr;
	int flags;

	clilen = sizeof(client_addr);
	fd = accept(sock, (struct sockaddr*)&client_addr, &clilen);
	if (fd == -1) {
		log_perror_err("accept() failed for new client");
		dosigterm(SIGTERM);
	}
	;

	if (clin >= MAX_CLIENTS) {
		log_error("connection rejected (too many clients)");
		shutdown(fd, 2);
		close(fd);
		return;
	}
	nolinger(fd);
	flags = fcntl(fd, F_GETFL, 0);
	if (flags != -1)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (client_addr.sa_family == AF_UNIX) {
		cli_type[clin] = CT_LOCAL;
		log_notice("accepted new client on %s", lircdfile);
	} else if (client_addr.sa_family == AF_INET) {
		cli_type[clin] = CT_REMOTE;
		log_notice("accepted new client from %s",
			  inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr));
	} else {
		cli_type[clin] = 0;     /* what? */
	}
	clis[clin] = fd;
	if (!use_hw()) {
		if (curr_driver->init_func) {
			if (!curr_driver->init_func()) {
				log_warn("Failed to initialize hardware");
			/* Don't exit here, otherwise lirc
			 * bails out, and lircd exits, making
			 * it impossible to connect to when we
			 * have a device actually plugged
			 * in. */
			} else {
				setup_hardware();
			}
		}
	}
	clin++;
}

int add_peer_connection(const char* server_arg)
{
	char* sep;
	struct servent* service;
	char server[strlen(server_arg) + 1];

	strncpy(server, server_arg, sizeof(server) - 1);

	if (peern < MAX_PEERS) {
		peers[peern] = (struct peer_connection*) malloc(sizeof(
						    struct peer_connection));
		if (peers[peern] != NULL) {
			gettimeofday(&peers[peern]->reconnect, NULL);
			peers[peern]->connection_failure = 0;
			sep = strchr(server, ':');
			if (sep != NULL) {
				*sep = 0;
				sep++;
				peers[peern]->host = strdup(server);
				service = getservbyname(sep, "tcp");
				if (service) {
					peers[peern]->port = ntohs(service->s_port);
				} else {
					long p;
					char* endptr;

					p = strtol(sep, &endptr, 10);
					if (!*sep || *endptr || p < 1 || p > USHRT_MAX) {
						fprintf(stderr, "%s: bad port number \"%s\"\n", progname, sep);
						return 0;
					}

					peers[peern]->port = (unsigned short int)p;
				}
			} else {
				peers[peern]->host = strdup(server);
				peers[peern]->port = LIRC_INET_PORT;
			}
			if (peers[peern]->host == NULL)
				fprintf(stderr, "%s: out of memory\n", progname);
		} else {
			fprintf(stderr, "%s: out of memory\n", progname);
			return 0;
		}
		peers[peern]->socket = -1;
		peern++;
		return 1;
	}
	fprintf(stderr, "%s: too many client connections\n", progname);
	return 0;
}


void connect_to_peer(peer_connection* peer)
{
	int r;
	char service[64];
	struct addrinfo* addrinfos;
	struct addrinfo* a;
	int enable = 1;

	snprintf(service, sizeof(service), "%d", peer->port);
	peer->socket = socket(AF_INET, SOCK_STREAM, 0);
	r = getaddrinfo(peer->host, service, NULL, &addrinfos);
	if (r != 0) {
		log_perror_err("Name lookup failure connecting to %s",
			       peer->host);
		goto errexit;
	}
	(void)setsockopt(peer->socket,
			 SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
	for (a = addrinfos; a != NULL; a = a->ai_next) {
		r = connect(peer->socket, a->ai_addr, a->ai_addrlen);
		if (r >= 0)
			break;
	}
	freeaddrinfo(addrinfos);
	if (r == -1) {
		log_perror_err("Cannot connect to %s", peer->host);
		goto errexit;
	}
	log_notice("Connected to %s", peer->host);
	peer->connection_failure = 0;
	return;

errexit:
	peer->connection_failure++;
	gettimeofday(&peer->reconnect, NULL);
	peer->reconnect.tv_sec += 5 * peer->connection_failure;
	close(peer->socket);
	peer->socket = -1;
	return;
}


void connect_to_peers(void)
{
	int i;
	struct timeval now;

	gettimeofday(&now, NULL);
	for (i = 0; i < peern; i++) {
		if (peers[i]->socket != -1)
			continue;
		/* some timercmp() definitions don't work with <= */
		if (timercmp(&peers[i]->reconnect, &now, <)) {
			connect_to_peer(peers[i]);
		}
	}
}


int get_peer_message(struct peer_connection* peer)
{
	int length;
	char buffer[PACKET_SIZE + 1];
	char* end;
	int i;

	length = read_timeout(peer->socket, buffer, PACKET_SIZE, 0);
	if (length) {
		buffer[length] = 0;
		end = strrchr(buffer, '\n');
		if (end == NULL || end[1] != 0) {
			log_error("bad send packet: \"%s\"", buffer);
			/* remove clients that behave badly */
			return 0;
		}
		end++;          /* include the \n */
		end[0] = 0;
		length = strlen(buffer);
		log_trace("received peer message: \"%s\"", buffer);
		for (i = 0; i < clin; i++) {
			/* don't relay messages to remote clients */
			if (cli_type[i] == CT_REMOTE)
				continue;
			log_trace("writing to client %d", i);
			if (write_socket(clis[i], buffer, length) < length) {
				remove_client(clis[i]);
				i--;
			}
		}
	}

	if (length == 0)        /* EOF: connection closed by client */
		return 0;
	return 1;
}

void start_server(mode_t permission, int nodaemon, loglevel_t loglevel)
{
	struct sockaddr_un serv_addr;
	struct sockaddr_in serv_addr_in;
	struct stat s;
	int ret;
	int new_socket = 1;
	int fd;

#ifdef HAVE_SYSTEMD
	int n;
#endif

	lirc_log_open("lircd", nodaemon, loglevel);

	/* create pid lockfile in /var/run */
	fd = open(pidfile, O_RDWR | O_CREAT, 0644);
	if (fd > 0)
		pidf = fdopen(fd, "r+");
	if (fd == -1 || pidf == NULL) {
		perrorf("can't open or create %s", pidfile);
		exit(EXIT_FAILURE);
	}
	if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
		pid_t otherpid;

		if (fscanf(pidf, "%d\n", &otherpid) > 0) {
			fprintf(stderr, "%s: there seems to already be a lircd process with pid %d\n", progname,
				otherpid);
			fprintf(stderr, "%s: otherwise delete stale lockfile %s\n", progname, pidfile);
		} else {
			fprintf(stderr, "%s: invalid %s encountered\n", progname, pidfile);
		}
		exit(EXIT_FAILURE);
	}
	(void)fcntl(fd, F_SETFD, FD_CLOEXEC);
	rewind(pidf);
	(void)fprintf(pidf, "%d\n", getpid());
	(void)fflush(pidf);
	if (ftruncate(fileno(pidf), ftell(pidf)) != 0)
		log_perror_warn("lircd: ftruncate()");
	ir_remote_init(options_getboolean("lircd:dynamic-codes"));

	/* create socket */
	sockfd = -1;
	do_shutdown = 0;
#ifdef HAVE_SYSTEMD
	n = sd_listen_fds(0);
	if (n > 1) {
		fprintf(stderr, "Too many file descriptors received.\n");
		goto start_server_failed0;
	} else if (n == 1) {
		sockfd = SD_LISTEN_FDS_START + 0;
	}
#endif
	if (sockfd == -1) {
		sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sockfd == -1) {
			perror("Could not create socket");
			goto start_server_failed0;
		}
		do_shutdown = 1;

		/*
		 * get owner, permissions, etc.
		 * so new socket can be the same since we
		 * have to delete the old socket.
		 */
		ret = stat(lircdfile, &s);
		if (ret == -1 && errno != ENOENT) {
			perrorf("Could not get file information for %s\n",
				lircdfile);
			goto start_server_failed1;
		}
		if (ret != -1) {
			new_socket = 0;
			ret = unlink(lircdfile);
			if (ret == -1) {
				perrorf("Could not delete %s", lircdfile);
				goto start_server_failed1;
			}
		}

		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path, lircdfile);
		if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
			perrorf("Could not assign address to socket%s", lircdfile);
			goto start_server_failed1;
		}

		if (new_socket ? chmod(lircdfile, permission)
		    : (chmod(lircdfile, s.st_mode) == -1 || chown(lircdfile, s.st_uid, s.st_gid) == -1)
		    ) {
			perrorf("Could not set file permissions on %s", lircdfile);
			goto start_server_failed1;
		}

		listen(sockfd, 3);
	}
	nolinger(sockfd);

	if (useuinput)
		uinputfd = setup_uinputfd(progname);
	drop_privileges();
	if (listen_tcpip) {
		int enable = 1;

		/* create socket */
		sockinet = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
		if (sockinet == -1) {
			perror("Could not create TCP/IP socket");
			goto start_server_failed1;
		}
		(void)setsockopt(sockinet, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
		serv_addr_in.sin_family = AF_INET;
		serv_addr_in.sin_addr = address;
		serv_addr_in.sin_port = htons(port);

		if (bind(sockinet, (struct sockaddr*)&serv_addr_in, sizeof(serv_addr_in)) == -1) {
			perror("could not assign address to socket");
			goto start_server_failed2;
		}

		listen(sockinet, 3);
		nolinger(sockinet);
	}
	log_trace("started server socket");
	return;

start_server_failed2:
	if (listen_tcpip)
		close(sockinet);
start_server_failed1:
	close(sockfd);
start_server_failed0:
	fclose(pidf);
	(void)unlink(pidfile);
	exit(EXIT_FAILURE);
}


void daemonize(void)
{
	if (daemon(0, 0) == -1) {
		log_perror_err("daemon() failed");
		dosigterm(SIGTERM);
	}
	umask(0);
	rewind(pidf);
	fprintf(pidf, "%d\n", getpid());
	fflush(pidf);
	if (ftruncate(fileno(pidf), ftell(pidf)) != 0)
		log_perror_warn("lircd: ftruncate()");
	daemonized = 1;
}


int send_success(int fd, char* message)
{
	log_debug("Sending success");
	if (!
	    (write_socket_len(fd, protocol_string[P_BEGIN]) && write_socket_len(fd, message)
	     && write_socket_len(fd, protocol_string[P_SUCCESS]) && write_socket_len(fd, protocol_string[P_END])))
		return 0;
	return 1;
}


int send_error(int fd, char* message, const char* format_str, ...)
{
	log_debug("Sending error");
	char lines[4], buffer[PACKET_SIZE + 1];
	int i, n, len;
	va_list ap;
	char* s1;
	char* s2;
	char buffer2[PACKET_SIZE + 2];

	va_start(ap, format_str);
	vsprintf(buffer, format_str, ap);
	va_end(ap);

	s1 = strrchr(message, '\n');
	s2 = strrchr(buffer, '\n');
	if (s1 != NULL)
		s1[0] = 0;
	if (s2 != NULL)
		s2[0] = 0;
	log_error("error processing command: %s", message);
	log_error("%s", buffer);
	if (s1 != NULL)
		s1[0] = '\n';
	snprintf(buffer2, sizeof(buffer2), "%s\n", buffer);

	n = 0;
	len = strlen(buffer2);
	for (i = 0; i < len; i++)
		if (buffer2[i] == '\n')
			n++;
	sprintf(lines, "%d\n", n);

	if (!(write_socket_len(fd, protocol_string[P_BEGIN])
	      && write_socket_len(fd, message)
	      && write_socket_len(fd, protocol_string[P_ERROR])
	      && write_socket_len(fd, protocol_string[P_DATA])
	      && write_socket_len(fd, lines)
	      && write_socket_len(fd, buffer2)
	      && write_socket_len(fd, protocol_string[P_END])))
		return 0;
	return 1;
}


void sigalrm(int sig)
{
	alrm = 1;
}


static void schedule_repeat_timer (struct timespec* last)
{
	unsigned long secs;
	lirc_t usecs, gap, diff;
	struct timespec current;
	struct itimerval repeat_timer;
	gap = send_buffer_sum() + repeat_remote->min_remaining_gap;
	clock_gettime (CLOCK_MONOTONIC, &current);
	secs = current.tv_sec - last->tv_sec;
	diff = 1000000 * secs + (current.tv_nsec - last->tv_nsec) / 1000;
	usecs = (diff < gap ? gap - diff : 0);
	if (usecs < 10)
		usecs = 10;
	log_trace("alarm in %lu usecs", (unsigned long)usecs);
	repeat_timer.it_value.tv_sec = 0;
	repeat_timer.it_value.tv_usec = usecs;
	repeat_timer.it_interval.tv_sec = 0;
	repeat_timer.it_interval.tv_usec = 0;

	setitimer(ITIMER_REAL, &repeat_timer, NULL);
}

void dosigalrm(int sig)
{
	if (repeat_remote->last_code != repeat_code) {
		/* we received a different code from the original
		 * remote control we could repeat the wrong code so
		 * better stop repeating */
		if (repeat_fd != -1)
			send_error(repeat_fd, repeat_message, "repeating interrupted\n");

		repeat_remote = NULL;
		repeat_code = NULL;
		repeat_fd = -1;
		if (repeat_message != NULL) {
			free(repeat_message);
			repeat_message = NULL;
		}
		if (!use_hw() && curr_driver->deinit_func)
			curr_driver->deinit_func();
		return;
	}
	if (repeat_code->next == NULL
	    || (repeat_code->transmit_state != NULL && repeat_code->transmit_state->next == NULL))
		repeat_remote->repeat_countdown--;
	struct timespec before_send;
	clock_gettime (CLOCK_MONOTONIC, &before_send);
	if (send_ir_ncode(repeat_remote, repeat_code, 1) && repeat_remote->repeat_countdown > 0) {
		schedule_repeat_timer(&before_send);
		return;
	}
	repeat_remote = NULL;
	repeat_code = NULL;
	if (repeat_fd != -1) {
		send_success(repeat_fd, repeat_message);
		free(repeat_message);
		repeat_message = NULL;
		repeat_fd = -1;
	}
	if (!use_hw() && curr_driver->deinit_func)
		curr_driver->deinit_func();
}


int parse_rc(int fd,
	     char* message, char* arguments,
	     struct ir_remote** remote, struct ir_ncode** code,
	     unsigned int* reps, int n, int* err)
{
	char* name = NULL;
	char* command = NULL;
	char* repeats;
	char* end_ptr = NULL;

	*remote = NULL;
	*code = NULL;
	*err = 1;
	if (arguments == NULL)
		goto arg_check;

	name = strtok(arguments, WHITE_SPACE);
	if (name == NULL)
		goto arg_check;
	*remote = get_ir_remote(remotes, name);
	if (*remote == NULL)
		return send_error(fd, message, "unknown remote: \"%s\"\n", name);
	command = strtok(NULL, WHITE_SPACE);
	if (command == NULL)
		goto arg_check;
	*code = get_code_by_name(*remote, command);
	if (*code == NULL)
		return send_error(fd, message, "unknown command: \"%s\"\n", command);
	if (reps != NULL) {
		repeats = strtok(NULL, WHITE_SPACE);
		if (repeats != NULL) {
			*reps = strtol(repeats, &end_ptr, 10);
			if (*end_ptr || *reps < 0)
				return send_error(fd, message, "bad send packet (reps/eol)\n");
			if (*reps > repeat_max)
				return send_error
					       (fd, message, "too many repeats: \"%d\" > \"%u\"\n", *reps, repeat_max);
		} else {
			*reps = -1;
		}
	}
	if (strtok(NULL, WHITE_SPACE) != NULL)
		return send_error(fd, message, "bad send packet (trailing ws)\n");
arg_check:
	if (n > 0 && *remote == NULL)
		return send_error(fd, message, "remote missing\n");
	if (n > 1 && *code == NULL)
		return send_error(fd, message, "code missing\n");
	*err = 0;
	return 1;
}


int send_remote_list(int fd, char* message)
{
	char buffer[PACKET_SIZE + 1];
	struct ir_remote* all;
	int n, len;

	n = 0;
	all = remotes;
	while (all) {
		n++;
		all = all->next;
	}

	if (!
	    (write_socket_len(fd, protocol_string[P_BEGIN]) && write_socket_len(fd, message)
	     && write_socket_len(fd, protocol_string[P_SUCCESS])))
		return 0;

	if (n == 0)
		return write_socket_len(fd, protocol_string[P_END]);
	sprintf(buffer, "%d\n", n);
	if (!(write_socket_len(fd, protocol_string[P_DATA]) && write_socket_len(fd, buffer)))
		return 0;

	all = remotes;
	while (all) {
		len = snprintf(buffer, PACKET_SIZE + 1, "%s\n", all->name);
		if (len >= PACKET_SIZE + 1)
			len = sprintf(buffer, "name_too_long\n");
		if (write_socket(fd, buffer, len) < len)
			return 0;
		all = all->next;
	}
	return write_socket_len(fd, protocol_string[P_END]);
}

int send_remote(int fd, char* message, struct ir_remote* remote)
{
	struct ir_ncode* codes;
	char buffer[PACKET_SIZE + 1];
	int n, len;

	n = 0;
	codes = remote->codes;
	if (codes != NULL) {
		while (codes->name != NULL) {
			n++;
			codes++;
		}
	}

	if (!
	    (write_socket_len(fd, protocol_string[P_BEGIN]) && write_socket_len(fd, message)
	     && write_socket_len(fd, protocol_string[P_SUCCESS])))
		return 0;
	if (n == 0)
		return write_socket_len(fd, protocol_string[P_END]);
	sprintf(buffer, "%d\n", n);
	if (!(write_socket_len(fd, protocol_string[P_DATA]) && write_socket_len(fd, buffer)))
		return 0;

	codes = remote->codes;
	while (codes->name != NULL) {
		len = snprintf(buffer, PACKET_SIZE, "%016llx %s\n", (unsigned long long)codes->code, codes->name);
		if (len >= PACKET_SIZE + 1)
			len = sprintf(buffer, "code_too_long\n");
		if (write_socket(fd, buffer, len) < len)
			return 0;
		codes++;
	}
	return write_socket_len(fd, protocol_string[P_END]);
}

int send_name(int fd, char* message, struct ir_ncode* code)
{
	char buffer[PACKET_SIZE + 1];
	int len;

	if (!
	    (write_socket_len(fd, protocol_string[P_BEGIN]) && write_socket_len(fd, message)
	     && write_socket_len(fd, protocol_string[P_SUCCESS]) && write_socket_len(fd, protocol_string[P_DATA])))
		return 0;
	len = snprintf(buffer, PACKET_SIZE, "1\n%016llx %s\n", (unsigned long long)code->code, code->name);
	if (len >= PACKET_SIZE + 1)
		len = sprintf(buffer, "1\ncode_too_long\n");
	if (write_socket(fd, buffer, len) < len)
		return 0;
	return write_socket_len(fd, protocol_string[P_END]);
}

static int list(int fd, char* message, char* arguments)
{
	struct ir_remote* remote;
	struct ir_ncode* code;
	int err;

	if (parse_rc(fd, message, arguments, &remote, &code, 0, 0, &err) == 0)
		return 0;
	if (err)
		return 1;

	if (remote == NULL)
		return send_remote_list(fd, message);
	if (code == NULL)
		return send_remote(fd, message, remote);
	return send_name(fd, message, code);
}

static int set_transmitters(int fd, char* message, char* arguments)
{
	char* next_arg = NULL;
	char* end_ptr;
	__u32 next_tx_int = 0;
	__u32 next_tx_hex = 0;
	__u32 channels = 0;
	int retval = 0;
	unsigned int i;

	if (arguments == NULL)
		goto string_error;
	if (curr_driver->send_mode == 0)
		return send_error(fd, message, "hardware does not support sending\n");
	if (curr_driver->drvctl_func == NULL || !(curr_driver->features & LIRC_CAN_SET_TRANSMITTER_MASK))
		return send_error(fd, message, "hardware does not support multiple transmitters\n");

	next_arg = strtok(arguments, WHITE_SPACE);
	if (next_arg == NULL)
		goto string_error;
	do {
		next_tx_int = strtoul(next_arg, &end_ptr, 10);
		if (*end_ptr || next_tx_int == 0 || (next_tx_int == ULONG_MAX && errno == ERANGE))
			return send_error(fd, message, "invalid argument\n");
		if (next_tx_int > MAX_TX)
			return send_error(fd, message, "cannot support more than %d transmitters\n", MAX_TX);
		next_tx_hex = 1;
		for (i = 1; i < next_tx_int; i++)
			next_tx_hex = next_tx_hex << 1;
		channels |= next_tx_hex;
	} while ((next_arg = strtok(NULL, WHITE_SPACE)) != NULL);

	retval = curr_driver->drvctl_func(LIRC_SET_TRANSMITTER_MASK, &channels);
	if (retval < 0)
		return send_error(fd, message, "error - could not set transmitters\n");
	if (retval > 0)
		return send_error(fd, message, "error - maximum of %d transmitters\n", retval);
	return send_success(fd, message);

string_error:
	return send_error(fd, message, "no arguments given\n");
}


void broadcast_message(const char* message)
{
	int len, i;

	len = strlen(message);

	for (i = 0; i < clin; i++) {
		log_trace("writing to client %d: %s", i, message);
		if (write_socket(clis[i], message, len) < len) {
			remove_client(clis[i]);
			i--;
		}
	}
}


static int simulate(int fd, char* message, char* arguments)
{
	int i;
	char* sim;
	char* s;
	char* space;

	log_debug("simulate: enter");

	if (!allow_simulate)
		return send_error(fd, message, "SIMULATE command is disabled\n");
	if (arguments == NULL)
		return send_error(fd, message, "no arguments given\n");

	s = arguments;
	for (i = 0; i < 16; i++, s++)
		if (!isxdigit(*s))
			goto simulate_invalid_event;
	if (*s != ' ')
		goto simulate_invalid_event;
	s++;
	if (*s == ' ')
		goto simulate_invalid_event;
	for (; *s != ' '; s++)
		if (!isxdigit(*s))
			goto simulate_invalid_event;
	s++;
	space = strchr(s, ' ');
	if (space == NULL || space == s)
		goto simulate_invalid_event;
	s = space + 1;
	space = strchr(s, ' ');
	if (strlen(s) == 0 || space != NULL)
		goto simulate_invalid_event;

	sim = (char*) malloc(strlen(arguments) + 1 + 1);
	if (sim == NULL)
		return send_error(fd, message, "out of memory\n");
	strcpy(sim, arguments);
	strcat(sim, "\n");
	broadcast_message(sim);
	free(sim);

	return send_success(fd, message);
simulate_invalid_event:
	return send_error(fd, message, "invalid event\n");
}

static int send_once(int fd, char* message, char* arguments)
{
	return send_core(fd, message, arguments, 1);
}

static int send_start(int fd, char* message, char* arguments)
{
	return send_core(fd, message, arguments, 0);
}

static int send_core(int fd, char* message, char* arguments, int once)
{
	struct ir_remote* remote;
	struct ir_ncode* code;
	unsigned int reps;
	int err;

	log_debug("Sending once, msg: %s, args: %s, once: %d",
		  message, arguments, once);
	if (curr_driver->send_mode == 0)
		return send_error(fd, message, "hardware does not support sending\n");

	if (parse_rc(fd, message, arguments, &remote, &code, once ? &reps : NULL, 2, &err) == 0)
		return 0;
	if (err)
		return 1;

	if (once) {
		if (repeat_remote != NULL)
			return send_error(fd, message, "busy: repeating\n");
	} else {
		if (repeat_remote != NULL)
			return send_error(fd, message, "already repeating\n");
	}
	if (has_toggle_mask(remote))
		remote->toggle_mask_state = 0;
	if (has_toggle_bit_mask(remote))
		remote->toggle_bit_mask_state = (remote->toggle_bit_mask_state ^ remote->toggle_bit_mask);
	code->transmit_state = NULL;
	struct timespec before_send;
	clock_gettime (CLOCK_MONOTONIC, &before_send);
	if (!send_ir_ncode(remote, code, 1))
		return send_error(fd, message, "transmission failed\n");
	gettimeofday(&remote->last_send, NULL);
	remote->last_code = code;
	if (once)
		remote->repeat_countdown = max(remote->repeat_countdown, reps);
	else
		/* you've been warned, now we have a limit */
		remote->repeat_countdown = repeat_max;
	if (remote->repeat_countdown > 0 || code->next != NULL) {
		repeat_remote = remote;
		repeat_code = code;
		if (once) {
			repeat_message = strdup(message);
			if (repeat_message == NULL) {
				repeat_remote = NULL;
				repeat_code = NULL;
				return send_error(fd, message, "out of memory\n");
			}
			repeat_fd = fd;
		} else if (!send_success(fd, message)) {
			repeat_remote = NULL;
			repeat_code = NULL;
			return 0;
		}
		schedule_repeat_timer(&before_send);
		return 1;
	} else {
		return send_success(fd, message);
	}
}

static int send_stop(int fd, char* message, char* arguments)
{
	struct ir_remote* remote;
	struct ir_ncode* code;
	struct itimerval repeat_timer;
	int err;

	if (parse_rc(fd, message, arguments, &remote, &code, 0, 0, &err) == 0)
		return 0;
	if (err)
		return 1;

	if (repeat_remote && repeat_code) {
		int done;

		if (remote && strcasecmp(remote->name, repeat_remote->name) != 0)
			return send_error(fd, message, "specified remote does not match\n");
		if (code && strcasecmp(code->name, repeat_code->name) != 0)
			return send_error(fd, message, "specified code does not match\n");

		done = repeat_max - repeat_remote->repeat_countdown;
		if (done < repeat_remote->min_repeat) {
			/* we still have some repeats to do */
			repeat_remote->repeat_countdown = repeat_remote->min_repeat - done;
			return send_success(fd, message);
		}
		repeat_timer.it_value.tv_sec = 0;
		repeat_timer.it_value.tv_usec = 0;
		repeat_timer.it_interval.tv_sec = 0;
		repeat_timer.it_interval.tv_usec = 0;

		setitimer(ITIMER_REAL, &repeat_timer, NULL);

		repeat_remote->toggle_mask_state = 0;
		repeat_remote = NULL;
		repeat_code = NULL;
		/* clin!=0, so we don't have to deinit hardware */
		alrm = 0;
		return send_success(fd, message);
	} else {
		return send_error(fd, message, "not repeating\n");
	}
}


static int version(int fd, char* message, char* arguments)
{
	char buffer[PACKET_SIZE + 1];

	sprintf(buffer, "1\n%s\n", VERSION);
	if (!(write_socket_len(fd, protocol_string[P_BEGIN]) &&
	      write_socket_len(fd, message) && write_socket_len(fd, protocol_string[P_SUCCESS])
	      && write_socket_len(fd, protocol_string[P_DATA]) && write_socket_len(fd, buffer)
	      && write_socket_len(fd, protocol_string[P_END])))
		return 0;
	return 1;
}


static int drv_option(int fd, char* message, char* arguments)
{
	struct option_t option;
	int r;

	r = sscanf(arguments, "%32s %64s", option.key, option.value);
	if (r != 2) {
		return send_error(fd, message,
				  "Illegal argument (protocol error): %s",
				  arguments);
	}
	r = curr_driver->drvctl_func(DRVCTL_SET_OPTION, (void*)&option);
	if (r != 0) {
		log_warn("Cannot set driver option");
		return send_error(fd, message,
				  "Cannot set driver option, code: %d", errno);
	}
	return send_success(fd, message);
}


static int set_inputlog(int fd, char* message, char* arguments)
{
	char buff[128];
	FILE* f;
	int r;

	if (arguments) {
		r = sscanf(arguments, "%128s", buff);
		if (r != 1) {
			return send_error(
				fd, message,
				"Illegal argument (protocol error): %s",
				arguments
			);
		}
	}
	if (!arguments || strcasecmp(buff, "null") == 0) {
		rec_buffer_set_logfile(NULL);
		return send_success(fd, message);
	}
	f = fopen(buff, "w");
	if (f == NULL) {
		log_warn("Cannot open input logfile: %s", buff);
		return send_error(fd, message,
				  "Cannot open input logfile: %s (errno: %d)",
				  buff, errno);
	}
	rec_buffer_set_logfile(f);
	return send_success(fd, message);
}


int get_command(int fd)
{
	int length;
	char buffer[PACKET_SIZE + 1], backup[PACKET_SIZE + 1];
	char* end;
	int packet_length, i;
	char* directive;

	length = read_timeout(fd, buffer, PACKET_SIZE, 0);
	packet_length = 0;
	while (length > packet_length) {
		buffer[length] = 0;
		end = strchr(buffer, '\n');
		if (end == NULL) {
			log_error("bad send packet: \"%s\"", buffer);
			/* remove clients that behave badly */
			return 0;
		}
		end[0] = 0;
		log_trace("received command: \"%s\"", buffer);
		packet_length = strlen(buffer) + 1;

		strcpy(backup, buffer);
		strcat(backup, "\n");

		/* remove DOS line endings */
		end = strrchr(buffer, '\r');
		if (end && end[1] == 0)
			*end = 0;

		directive = strtok(buffer, WHITE_SPACE);
		if (directive == NULL) {
			if (!send_error(fd, backup, "bad send packet\n"))
				return 0;
			goto skip;
		}
		for (i = 0; directives[i].name != NULL; i++) {
			if (strcasecmp(directive, directives[i].name) == 0) {
				if (!directives[i].function(fd, backup, strtok(NULL, "")))
					return 0;
				goto skip;
			}
		}

		if (!send_error(fd, backup, "unknown directive: \"%s\"\n", directive))
			return 0;
skip:
		if (length > packet_length) {
			int new_length;

			memmove(buffer, buffer + packet_length, length - packet_length + 1);
			if (strchr(buffer, '\n') == NULL) {
				new_length =
					read_timeout(fd, buffer + length - packet_length,
						     PACKET_SIZE - (length - packet_length), 5);
				if (new_length > 0)
					length = length - packet_length + new_length;
				else
					length = new_length;
			} else {
				length -= packet_length;
			}
			packet_length = 0;
		}
	}

	if (length == 0)        /* EOF: connection closed by client */
		return 0;
	return 1;
}

void input_message(const char* message, const char* remote_name, const char* button_name, int reps, int release)
{
	const char* release_message;
	const char* release_remote_name;
	const char* release_button_name;

	release_message = check_release_event(&release_remote_name, &release_button_name);
	if (release_message)
		input_message(release_message, release_remote_name, release_button_name, 0, 1);

	if (!release || userelease)
		broadcast_message(message);

#ifdef __linux__
	if (uinputfd == -1 || reps >= 2)
		return;

	linux_input_code input_code;

	if (get_input_code(button_name, &input_code) != -1) {
		struct input_event event;

		memset(&event, 0, sizeof(event));
		event.type = EV_KEY;
		event.code = input_code;
		event.value = release ? 0 : (reps > 0 ? 2 : 1);
		if (write(uinputfd, &event, sizeof(event)) != sizeof(event)) {
			log_perror_err("writing to uinput failed");
		}

		/* Need to write sync event */
		memset(&event, 0, sizeof(event));
		event.type = EV_SYN;
		event.code = SYN_REPORT;
		event.value = 0;
		if (write(uinputfd, &event, sizeof(event)) != sizeof(event)) {
			log_perror_err("writing EV_SYN to uinput failed");
		}
	} else {
		log_debug(
			  "Dropping non-standard symbol %s in uinput mode",
			  button_name == NULL ? "Null" : button_name);
	}
#endif
}




void free_old_remotes(void)
{
	struct ir_remote* scan_remotes;
	struct ir_remote* found;
	struct ir_ncode* code;
	const char* release_event;
	const char* release_remote_name;
	const char* release_button_name;

	if (get_decoding() == free_remotes)
		return;

	release_event = release_map_remotes(free_remotes, remotes, &release_remote_name, &release_button_name);
	if (release_event != NULL)
		input_message(release_event, release_remote_name, release_button_name, 0, 1);
	if (last_remote != NULL) {
		if (is_in_remotes(free_remotes, last_remote)) {
			log_info("last_remote found");
			found = get_ir_remote(remotes, last_remote->name);
			if (found != NULL) {
				code = get_code_by_name(found, last_remote->last_code->name);
				if (code != NULL) {
					found->reps = last_remote->reps;
					found->toggle_bit_mask_state = last_remote->toggle_bit_mask_state;
					found->min_remaining_gap = last_remote->min_remaining_gap;
					found->max_remaining_gap = last_remote->max_remaining_gap;
					found->last_send = last_remote->last_send;
					last_remote = found;
					last_remote->last_code = code;
					log_info("mapped last_remote");
				}
			}
		} else {
			last_remote = NULL;
		}
	}
	/* check if last config is still needed */
	found = NULL;
	if (repeat_remote != NULL) {
		scan_remotes = free_remotes;
		while (scan_remotes != NULL) {
			if (repeat_remote == scan_remotes) {
				found = repeat_remote;
				break;
			}
			scan_remotes = scan_remotes->next;
		}
		if (found != NULL) {
			found = get_ir_remote(remotes, repeat_remote->name);
			if (found != NULL) {
				code = get_code_by_name(found, repeat_code->name);
				if (code != NULL) {
					struct itimerval repeat_timer;

					repeat_timer.it_value.tv_sec = 0;
					repeat_timer.it_value.tv_usec = 0;
					repeat_timer.it_interval.tv_sec = 0;
					repeat_timer.it_interval.tv_usec = 0;

					found->last_code = code;
					found->last_send = repeat_remote->last_send;
					found->toggle_bit_mask_state = repeat_remote->toggle_bit_mask_state;
					found->min_remaining_gap = repeat_remote->min_remaining_gap;
					found->max_remaining_gap = repeat_remote->max_remaining_gap;

					setitimer(ITIMER_REAL, &repeat_timer, &repeat_timer);
					/* "atomic" (shouldn't be necessary any more) */
					repeat_remote = found;
					repeat_code = code;
					/* end "atomic" */
					setitimer(ITIMER_REAL, &repeat_timer, NULL);
					found = NULL;
				}
			} else {
				found = repeat_remote;
			}
		}
	}
	if (found == NULL && get_decoding() != free_remotes) {
		free_config(free_remotes);
		free_remotes = NULL;
	} else {
		log_trace("free_remotes still in use");
	}
}

struct pollfd_byname {
	struct pollfd sockfd;
	struct pollfd sockinet;
	struct pollfd curr_driver;
	struct pollfd clis[MAX_CLIENTS];
	struct pollfd peers[MAX_PEERS];
};

#define POLLFDS_SIZE (sizeof(struct pollfd_byname)/sizeof(pollfd))

static union {
	struct  pollfd_byname byname;
	struct  pollfd byindex[POLLFDS_SIZE];
} poll_fds;


static int mywaitfordata(unsigned long maxusec)
{
	int i;
	int ret, reconnect;
	struct timeval tv, start, now, timeout, release_time;
	loglevel_t oldlevel;

	while (1) {
		do {
			/* handle signals */
			if (term)
				dosigterm(termsig);
			/* never reached */
			if (hup) {
				dosighup(SIGHUP);
				hup = 0;
			}
			if (alrm) {
				dosigalrm(SIGALRM);
				alrm = 0;
			}
			memset(&poll_fds, 0, sizeof(poll_fds));
			for (i = 0; i < (int)POLLFDS_SIZE; i += 1)
				poll_fds.byindex[i].fd = -1;

			poll_fds.byname.sockfd.fd = sockfd;
			poll_fds.byname.sockfd.events = POLLIN;

			if (listen_tcpip) {
				poll_fds.byname.sockinet.fd = sockinet;
				poll_fds.byname.sockinet.events = POLLIN;
			}
			if (use_hw() && curr_driver->rec_mode != 0 && curr_driver->fd != -1) {
				poll_fds.byname.curr_driver.fd = curr_driver->fd;
				poll_fds.byname.curr_driver.events = POLLIN;
			}

			for (i = 0; i < clin; i++) {
				/* Ignore this client until codes have been
				 * sent and it will get an answer. Otherwise
				 * we could mix up answer packets and send
				 * them back in the wrong order. */
				if (clis[i] != repeat_fd) {
					poll_fds.byname.clis[i].fd = clis[i];
					poll_fds.byname.clis[i].events = POLLIN;
				}
			}
			timerclear(&tv);
			reconnect = 0;
			for (i = 0; i < peern; i++) {
				if (peers[i]->socket != -1) {
					poll_fds.byname.peers[i].fd = peers[i]->socket;
					poll_fds.byname.peers[i].events = POLLIN;
				} else if (timerisset(&tv)) {
					if (timercmp(&tv, &peers[i]->reconnect, >))
						tv = peers[i]->reconnect;
				} else {
					tv = peers[i]->reconnect;
				}
			}
			if (timerisset(&tv)) {
				gettimeofday(&now, NULL);
				if (timercmp(&now, &tv, >)) {
					timerclear(&tv);
				} else {
					timersub(&tv, &now, &start);
					tv = start;
				}
				reconnect = 1;
			}
			gettimeofday(&start, NULL);
			if (maxusec > 0) {
				tv.tv_sec = maxusec / 1000000;
				tv.tv_usec = maxusec % 1000000;
			}
			if (curr_driver->fd == -1 && use_hw()) {
				/* try to reconnect */
				timerclear(&timeout);
				timeout.tv_sec = 1;

				if (timercmp(&tv, &timeout, >)
				    || (!reconnect && !timerisset(&tv)))
					tv = timeout;
			}
			get_release_time(&release_time);
			if (timerisset(&release_time)) {
				gettimeofday(&now, NULL);
				if (timercmp(&now, &release_time, >)) {
					timerclear(&tv);
				} else {
					struct timeval gap;

					timersub(&release_time, &now, &gap);
					if (!(timerisset(&tv)
					      || reconnect)
					    || timercmp(&tv, &gap, >))
						tv = gap;
				}
			}
			if (timerisset(&tv) || timerisset(&release_time) || reconnect)
				ret = curl_poll((struct pollfd *) &poll_fds.byindex,
					         POLLFDS_SIZE,
					         tv.tv_sec * 1000 + tv.tv_usec / 1000);
			else
				ret = curl_poll((struct pollfd*)&poll_fds.byindex,
					         POLLFDS_SIZE,
						 -1);

			if (ret == -1 && errno != EINTR) {
				log_perror_err("curl_poll()() failed");
				raise(SIGTERM);
				continue;
			}
			gettimeofday(&now, NULL);
			if (timerisset(&release_time) && timercmp(&now, &release_time, >)) {
				const char* release_message;
				const char* release_remote_name;
				const char* release_button_name;

				release_message =
					trigger_release_event(&release_remote_name,
							      &release_button_name);
				if (release_message) {
					input_message(release_message,
						      release_remote_name,
						      release_button_name,
						      0, 1);
				}
			}
			if (free_remotes != NULL)
				free_old_remotes();
			if (maxusec > 0) {
				if (ret == 0)
					return 0;
				if (time_elapsed(&start, &now) >= maxusec)
					return 0;
				maxusec -= time_elapsed(&start, &now);
			}
			if (reconnect)
				connect_to_peers();
		} while (ret == -1 && errno == EINTR);

		if (curr_driver->fd == -1 && use_hw() && curr_driver->init_func) {
			oldlevel = loglevel;
			lirc_log_setlevel(LIRC_ERROR);
			curr_driver->init_func();
			setup_hardware();
			lirc_log_setlevel(oldlevel);
		}
		for (i = 0; i < clin; i++) {
			if (poll_fds.byname.clis[i].revents & POLLIN) {
				poll_fds.byname.clis[i].revents = 0;
				if (get_command(clis[i]) == 0) {
					remove_client(clis[i]);
					i--;
				}
			}
		}
		for (i = 0; i < peern; i++) {
			if (peers[i]->socket != -1 && poll_fds.byname.peers[i].revents & POLLIN) {
				poll_fds.byname.peers[i].revents = 0;
				if (get_peer_message(peers[i]) == 0) {
					shutdown(peers[i]->socket, 2);
					close(peers[i]->socket);
					peers[i]->socket = -1;
					peers[i]->connection_failure = 1;
					gettimeofday(&peers[i]->reconnect, NULL);
					peers[i]->reconnect.tv_sec += 5;
				}
			}
		}

		if (poll_fds.byname.sockfd.revents & POLLIN) {
			poll_fds.byname.sockfd.revents = 0;
			log_trace("registering local client");
			add_client(sockfd);
		}
		if (poll_fds.byname.sockinet.revents & POLLIN) {
			poll_fds.byname.sockinet.revents = 0;
			log_trace("registering inet client");
			add_client(sockinet);
		}
		if (use_hw() && curr_driver->rec_mode != 0
		    && curr_driver->fd != -1
		    && poll_fds.byname.curr_driver.revents & POLLIN) {
			register_input();
			/* we will read later */
			return 1;
		}
	}
}

void loop(void)
{
	char* message;

	log_notice("lircd(%s) ready, using %s", curr_driver->name, lircdfile);
	if(useuinput) {
		// Don't wait for client to connect when using uinput (#161)
		if (curr_driver->init_func) {
			if (!curr_driver->init_func()) {
				log_warn("Failed to initialize hardware");
			}
		}
	}
	while (1) {
		(void)mywaitfordata(0);
		if (!curr_driver->rec_func)
			continue;
		message = curr_driver->rec_func(remotes);

		if (message != NULL) {
			const char* remote_name;
			const char* button_name;
			int reps;

			if (curr_driver->drvctl_func && (curr_driver->features & LIRC_CAN_NOTIFY_DECODE))
				curr_driver->drvctl_func(DRVCTL_NOTIFY_DECODE, NULL);

			get_release_data(&remote_name, &button_name, &reps);

			input_message(message, remote_name, button_name, reps, 0);
		}
	}
}


static int opt2host_port(const char*		optarg_arg,
			 struct in_addr*	address,
			 unsigned short*	port,
			 char*			errmsg)
{
	char optarg[strlen(optarg_arg) + 1];

	strcpy(optarg, optarg_arg);
	long p;
	char* endptr;
	char* sep = strchr(optarg, ':');
	const char* port_str = sep ? sep + 1 : optarg;

	p = strtol(port_str, &endptr, 10);
	if (!*optarg || *endptr || p < 1 || p > USHRT_MAX) {
		sprintf(errmsg,
			"%s: bad port number \"%s\"\n", progname, port_str);
		return -1;
	}
	*port = (unsigned short int)p;
	if (sep) {
		*sep = '\0';
		if (!inet_aton(optarg, address)) {
			sprintf(errmsg,
				"%s: bad address \"%s\"\n", progname, optarg);
			return -1;
		}
	}
	return 0;
}


static void lircd_add_defaults(void)
{
	char level[4];

	snprintf(level, sizeof(level), "%d", lirc_log_defaultlevel());

	const char* const defaults[] = {
		"lircd:nodaemon",	"False",
		"lircd:permission",	DEFAULT_PERMISSIONS,
		"lircd:driver",		"default",
		"lircd:device",		NULL,
		"lircd:listen",		NULL,
		"lircd:connect",	NULL,
		"lircd:output",		LIRCD,
		"lircd:pidfile",	PIDFILE,
		"lircd:logfile",	"syslog",
		"lircd:debug",		level,
		"lircd:release",	NULL,
		"lircd:allow-simulate",	"False",
		"lircd:dynamic-codes",	"False",
		"lircd:plugindir",	PLUGINDIR,
		"lircd:uinput",		"False",
		"lircd:repeat-max",	DEFAULT_REPEAT_MAX,
		"lircd:configfile",	LIRCDCFGFILE,
		"lircd:driver-options",	"",
		"lircd:effective-user",	"",

		(const char*)NULL,	(const char*)NULL
	};
	options_add_defaults(defaults);
}


int parse_peer_connections(const char* opt)
{
	char buff[256];
	static const char* const SEP = ", ";
	char* host;

	if (opt == NULL)
		return 1;
	strncpy(buff, opt, sizeof(buff) - 1);
	for (host = strtok(buff, SEP); host; host = strtok(NULL, SEP)) {
		if (!add_peer_connection(host))
			return 0;
	}
	return 1;
}


static void lircd_parse_options(int argc, char** const argv)
{
	int c;
	const char* optstring = "A:e:O:hvnp:H:d:o:U:P:l::L:c:r::aR:D::Y"
#       if defined(__linux__)
				"u"
#       endif
	;

	strncpy(progname, "lircd", sizeof(progname));
	optind = 1;
	lircd_add_defaults();
	while ((c = getopt_long(argc, argv, optstring, lircd_options, NULL))
	       != -1) {
		switch (c) {
		case 'h':
			fputs(help, stdout);
			exit(EXIT_SUCCESS);
		case 'v':
			printf("lircd %s\n", VERSION);
			exit(EXIT_SUCCESS);
		case 'e':
			if (getuid() != 0) {
				log_warn("Trying to set user while"
					 " not being root");
			}
			options_set_opt("lircd:effective-user", optarg);
			break;
		case 'O':
			break;
		case 'n':
			options_set_opt("lircd:nodaemon", "True");
			break;
		case 'p':
			options_set_opt("lircd:permission", optarg);
			break;
		case 'H':
			options_set_opt("lircd:driver", optarg);
			break;
		case 'd':
			options_set_opt("lircd:device", optarg);
			break;
		case 'P':
			options_set_opt("lircd:pidfile", optarg);
			break;
		case 'L':
			options_set_opt("lircd:logfile", optarg);
			break;
		case 'o':
			options_set_opt("lircd:output", optarg);
			break;
		case 'l':
			options_set_opt("lircd:listen",
					optarg ? optarg : "0.0.0.0:8765");
			break;
		case 'c':
			options_set_opt("lircd:connect", optarg);
			break;
		case 'D':
			loglevel_opt = (loglevel_t) options_set_loglevel(
				optarg ? optarg : "debug");
			if (loglevel_opt == LIRC_BADLEVEL) {
				fprintf(stderr, DEBUG_HELP, optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'a':
			options_set_opt("lircd:allow-simulate", "True");
			break;
		case 'r':
			options_set_opt("lircd:release", "True");
			options_set_opt("lircd:release_suffix",
					optarg ? optarg : LIRC_RELEASE_SUFFIX);
			break;
#               if defined(__linux__)
		case 'u':
			options_set_opt("lircd:uinput", "True");
			break;
#               endif
		case 'U':
			options_set_opt("lircd:plugindir", optarg);
			break;
		case 'R':
			options_set_opt("lircd:repeat-max", optarg);
			break;
		case 'Y':
			options_set_opt("lircd:dynamic-codes", "True");
			break;
		case 'A':
			options_set_opt("lircd:driver-options", optarg);
			break;
		default:
			printf("Usage: %s [options] [config-file]\n", progname);
			exit(EXIT_FAILURE);
		}
	}
	if (optind == argc - 1) {
		options_set_opt("lircd:configfile", argv[optind]);
	} else if (optind != argc) {
		fprintf(stderr, "%s: invalid argument count\n", progname);
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char** argv)
{
	struct sigaction act;
	mode_t permission;
	const char* device = NULL;
	char errmsg[128];
	const char* opt;

	address.s_addr = htonl(INADDR_ANY);
	hw_choose_driver(NULL);
	options_load(argc, argv, NULL, lircd_parse_options);
	opt = options_getstring("lircd:debug");
	if (options_set_loglevel(opt) == LIRC_BADLEVEL) {
		fprintf(stderr, "Bad configuration loglevel:%s\n", opt);
		fprintf(stderr, DEBUG_HELP, optarg);
		fprintf(stderr, "Falling back to 'info'\n");
	}
	opt = options_getstring("lircd:logfile");
	if (opt != NULL)
		lirc_log_set_file(opt);
	lirc_log_open("lircd", 0, LIRC_INFO);

	nodaemon = options_getboolean("lircd:nodaemon");
	opt = options_getstring("lircd:permission");
	if (oatoi(opt) == -1) {
		fprintf(stderr, "%s: Invalid mode %s\n", progname, opt);
		return EXIT_FAILURE;
	}
	permission = oatoi(opt);
	device = options_getstring("lircd:device");
	opt = options_getstring("lircd:driver");
	if (strcmp(opt, "help") == 0 || strcmp(opt, "?") == 0) {
		hw_print_drivers(stdout);
		return EXIT_SUCCESS;
	}
	if (hw_choose_driver(opt) != 0) {
		fprintf(stderr, "Driver `%s' not found or not loadable", opt);
		fprintf(stderr, " (wrong or missing -U/--plugindir?).\n");
		fputs("Use lirc-lsplugins(1) to list available drivers.\n",
                      stderr);
		hw_print_drivers(stderr);
		return EXIT_FAILURE;
	}
	curr_driver->open_func(device);
	opt = options_getstring("lircd:driver-options");
	if (opt != NULL)
		drv_handle_options(opt);
	pidfile = options_getstring("lircd:pidfile");
	lircdfile = options_getstring("lircd:output");
	opt = options_getstring("lircd:logfile");
	if (opt != NULL)
		lirc_log_set_file(opt);
	if (options_getstring("lircd:listen") != NULL) {
		listen_tcpip = 1;
		opt = options_getstring("lircd:listen");
		if (opt) {
			if (opt2host_port(opt, &address, &port, errmsg) != 0) {
				fputs(errmsg, stderr);
				return EXIT_FAILURE;
			}
		} else {
			port = LIRC_INET_PORT;
		}
	}
	opt = options_getstring("lircd:connect");
	if (!parse_peer_connections(opt))
		return(EXIT_FAILURE);
	loglevel_opt = (loglevel_t) options_getint("lircd:debug");
	userelease = options_getboolean("lircd:release");
	set_release_suffix(options_getstring("lircd:release_suffix"));
	allow_simulate = options_getboolean("lircd:allow-simulate");
#       if defined(__linux__)
	useuinput = options_getboolean("lircd:uinput");
	if (useuinput)
		log_warn("--uinput is deprecated, check the lircd manpage.");
#       endif
	repeat_max = options_getint("lircd:repeat-max");
	configfile = options_getstring("lircd:configfile");
	curr_driver->open_func(device);
	if (strcmp(curr_driver->name, "null") == 0 && peern == 0) {
		fprintf(stderr, "%s: there's no hardware I can use and no peers are specified\n", progname);
		return EXIT_FAILURE;
	}
	if (curr_driver->device != NULL && strcmp(curr_driver->device, lircdfile) == 0) {
		fprintf(stderr, "%s: refusing to connect to myself\n", progname);
		fprintf(stderr, "%s: device and output must not be the same file: %s\n", progname, lircdfile);
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, SIG_IGN);

	start_server(permission, nodaemon, loglevel_opt);

	act.sa_handler = sigterm;
	sigfillset(&act.sa_mask);
	act.sa_flags = SA_RESTART;      /* don't fiddle with EINTR */
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);

	act.sa_handler = sigalrm;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;      /* don't fiddle with EINTR */
	sigaction(SIGALRM, &act, NULL);

	act.sa_handler = dosigterm;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &act, NULL);

	remotes = NULL;
	config();               /* read config file */

	act.sa_handler = sighup;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;      /* don't fiddle with EINTR */
	sigaction(SIGHUP, &act, NULL);

	/* ready to accept connections */
	if (!nodaemon)
		daemonize();

	loop();

	/* never reached */
	return EXIT_SUCCESS;
}

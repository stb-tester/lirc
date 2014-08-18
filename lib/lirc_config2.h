
/* Default device in some  places, notably drivers.
 * Might be something else on Darwin(?), but all current
 * Linux systems should be using udev (i. e., not DEVFS).
 */
#ifdef LIRC_HAVE_DEVFS
#define LIRC_DRIVER_DEVICE      "/dev/lirc/0"
#else
#define LIRC_DRIVER_DEVICE      "/dev/lirc0"
#endif /* LIRC_HAVE_DEVFS */

#define LIRCD                   VARRUNDIR "/" PACKAGE "/" DEV_LIRCD
#define LIRCM                   VARRUNDIR "/" PACKAGE "/" DEV_LIRCM

#define LIRCDCFGFILE            SYSCONFDIR "/" PACKAGE "/" CFG_LIRCD
#define LIRCMDCFGFILE           SYSCONFDIR "/" PACKAGE "/" CFG_LIRCM

#define LIRCDOLDCFGFILE         SYSCONFDIR "/" CFG_LIRCD
#define LIRCMDOLDCFGFILE        SYSCONFDIR "/" CFG_LIRCM

#define LIRCRC_USER_FILE        "." CFG_LIRCRC
#define LIRCRC_ROOT_FILE        SYSCONFDIR "/" PACKAGE "/" CFG_LIRCRC
#define LIRCRC_OLD_ROOT_FILE    SYSCONFDIR "/" CFG_LIRCRC

#define LOGFILE                 LOCALSTATEDIR "/log/" LOG_LIRCD
#define LIRMAND_LOGFILE         LOCALSTATEDIR "/log/" LOG_LIRMAND

#define PIDFILE                 VARRUNDIR "/" PACKAGE "/" PID_LIRCD

#define LIRC_RELEASE_SUFFIX     "_UP"

/* Default directory for plugins/drivers. */
#define PLUGINDIR		LIBDIR  "/lirc/plugins"

/* Default options file path. */
#define LIRC_OPTIONS_PATH       "/etc/lirc/lirc_options.conf"

/* Environment variable overriding options file path. */
#define LIRC_OPTIONS_VAR        "LIRC_OPTIONS_PATH"

/* Default permissions for /var/run/lircd. */
#define DEFAULT_PERMISSIONS     "666"

/* Default timeout (ms) while waiting for socket. */
#define SOCKET_TIMEOUT          "5000"

/* Default for --repeat-max option. */
#define DEFAULT_REPEAT_MAX      "600"

/* IR transmission packet size. */
#define PACKET_SIZE             (256)

/* Environment variable holding defaults for PLUGINDIR. */
#define PLUGINDIR_VAR           "LIRC_PLUGINDIR"

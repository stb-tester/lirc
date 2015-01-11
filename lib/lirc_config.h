/**
 * @file lirc_config.h
 * @brief Local configuration constants not managed by autotools.
 * @ingroup private_api
 * @ingroup driver_api
 */

/**
 * If defined, duplicates log console output to walk-around systemd
 * not handling the LOGPERROR  openlog() flag correctly.
 */

// #define SYSTEMD_LOGPERROR_FIX

/** lircd socket file name - beneath $varrundir (default /var/run/lirc) */
#define DEV_LIRCD       "lircd"

/** lircmd socket file name - beneath $varrundir (default /var/run/lirc) */
#define DEV_LIRCM       "lircm"

/** lircd,conf  file name - beneath SYSCONFDIR (default /etc) */
#define CFG_LIRCD       "lircd.conf"

/** lircmd,conf  file name - beneath SYSCONFDIR (default /etc) */
#define CFG_LIRCM       "lircmd.conf"

/** config file names - beneath $HOME or SYSCONFDIR */
#define CFG_LIRCRC      "lircrc"

/** pid file */
#define PID_LIRCD       "lircd.pid"

/** default port number for UDP driver */
#define        LIRC_INET_PORT  8765


/* Default device in some  places, notably drivers.
 * Might be something else on Darwin(?), but all current
 * Linux systems should be using udev (i. e., not DEVFS).
 */
#ifdef LIRC_HAVE_DEVFS
#define LIRC_DRIVER_DEVICE      "/dev/lirc/0"
#else
#define LIRC_DRIVER_DEVICE      "/dev/lirc0"
#endif /* LIRC_HAVE_DEVFS */

/** Complete lircd socket path. */
#define LIRCD                   VARRUNDIR "/" PACKAGE "/" DEV_LIRCD
/** Complete lircmd socket path. */
#define LIRCM                   VARRUNDIR "/" PACKAGE "/" DEV_LIRCM

/** Complete lircd.conf  config file  path. */
#define LIRCDCFGFILE            SYSCONFDIR "/" PACKAGE "/" CFG_LIRCD

/** Complete lircmd.conf  config file  path. */
#define LIRCMDCFGFILE           SYSCONFDIR "/" PACKAGE "/" CFG_LIRCM

/** Compatibility: Old lircd.conf location. */
#define LIRCDOLDCFGFILE         SYSCONFDIR "/" CFG_LIRCD

/** Compatibility: Old lircmd.conf location. */
#define LIRCMDOLDCFGFILE        SYSCONFDIR "/" CFG_LIRCM

/** User lircrc file name.  */
#define LIRCRC_USER_FILE        "." CFG_LIRCRC

/** System-wide lircrc path. */
#define LIRCRC_ROOT_FILE        SYSCONFDIR "/" PACKAGE "/" CFG_LIRCRC

/** Compatibility: Old system-wide lircrc path.*/
#define LIRCRC_OLD_ROOT_FILE    SYSCONFDIR "/" CFG_LIRCRC

/** Complete pid file path. */
#define PIDFILE                 VARRUNDIR "/" PACKAGE "/" PID_LIRCD

/** Suffix added to release events. */
#define LIRC_RELEASE_SUFFIX     "_UP"

/** Default directory for plugins/drivers. */
#define PLUGINDIR               LIBDIR  "/lirc/plugins"

/** Default options file path. */
#define LIRC_OPTIONS_PATH       SYSCONFDIR "/lirc/lirc_options.conf"

/** Environment variable overriding options file path. */
#define LIRC_OPTIONS_VAR        "LIRC_OPTIONS_PATH"

/** Default permissions for /var/run/lircd. */
#define DEFAULT_PERMISSIONS     "666"

/** Default timeout (ms) while waiting for socket. */
#define SOCKET_TIMEOUT          "5000"

/** Default for --repeat-max option. */
#define DEFAULT_REPEAT_MAX      "600"

/** IR transmission packet size. */
#define PACKET_SIZE             (256)

/** Environment variable holding defaults for PLUGINDIR. */
#define PLUGINDIR_VAR           "LIRC_PLUGIN_PATH"

/** Bit manipulator in lirc_t, see lirc.h . Signals eof from remote. */
#define LIRC_EOF                0x08000000

/** Max number of plugins. */
#define MAX_PLUGINS             256

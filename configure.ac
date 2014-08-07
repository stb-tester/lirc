
dnl Process this file with autoconf to produce a configure script.
dnl configure.ac, (c)1999 Tom Wheeley <tw104@york.ac.uk>

AC_INIT([lirc], [0.9.2-devel])
AC_CONFIG_SRCDIR([daemons/lircd.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

CFLAGS="${CFLAGS--O2 -g -Wall}"

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(mknod, mknod, /bin/mknod)
AC_PATH_PROG(mkfifo, mkfifo, /usr/bin/mkfifo)
AC_PATH_PROG(depmod, depmod, /sbin/depmod, $PATH:/sbin)
AC_CHECK_PROG(HELP2MAN, help2man, [ok], [fail])
if test "$HELP2MAN" != ok; then
    AC_MSG_ERROR([help2man is required to build lirc])
fi
AC_PROG_LN_S
LT_INIT([disable-static])
AM_PATH_PYTHON(,, [:])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != ""])
LIBUSB_CONFIG="pkg-config libusb"

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS(fcntl.h limits.h sys/ioctl.h sys/time.h syslog.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(gethostname gettimeofday mkfifo select socket strdup \
        strerror strtoul snprintf strsep vsyslog)
AC_CHECK_FUNCS(daemon)
if test "$ac_cv_func_daemon" != yes; then
  daemon=""
  AC_CHECK_LIB(bsd,daemon,daemon="-lbsd")
  if test "$daemon" = ""; then
    AC_MSG_ERROR([*** daemon() function not available on this system, giving up])
  fi
fi

forkpty=""
AC_CHECK_FUNCS(forkpty)
if test "$ac_cv_func_forkpty" != yes; then
  AC_CHECK_LIB(util,forkpty,[
    forkpty="-lutil"
    AC_DEFINE(HAVE_FORKPTY)])
fi

vga_progs=
AC_CHECK_HEADER(vga.h,
  [AC_CHECK_HEADER(vgagl.h,
    [AC_CHECK_LIB(vga, vga_init,
      [AC_CHECK_LIB(vgagl, gl_setcontextvga,
        [AC_DEFINE(HAVE_LIBVGA)]
        vga_progs=smode2,,
        -lvga
      )]
    )]
  )]
)
AM_CONDITIONAL([BUILD_SMODE2],[ test x$vga_progs != x ])

x_progs="irxevent xmode2"
AC_PATH_XTRA
if test "$no_x" = "yes"; then
  x_progs=
fi
AM_CONDITIONAL([BUILD_XTOOLS],[ test "x$x_progs" != x ])

dnl getopt_long support
AC_CHECK_FUNCS(getopt_long,,
  [AC_CHECK_LIB(gnugetopt,getopt_long,LDFLAGS="${LDFLAGS} -lgnugetopt",
    [AC_MSG_ERROR([*** your libc doesn't provide the getopt_long function.
Therfore you need to have \"libgnugetopt\" installed to compile LIRC.
You can get a version from:
  http://www.inf.ethz.ch/~mringwal/downloads/libgnugetopt-1.2.tar.gz
Extract the package and do a \"./configure  && make && sudo make install\"
in its directory.
])]
  )]
)


dnl Determine default configuration based on OS
case "${host_os}" in
  darwin*)
    dnl Mac OS X configuration

    dnl PortAudio for mac os x
    portaudio_lib_other="-framework CoreAudio -framework AudioToolbox"

    dnl default devdir, on mac os x/darwin, /dev/ can not be used without kernel programming, so use a fifo somewhere else
    devdir_default="/usr/lirc/dev/"

    dnl default devdir
    devdir_default="/dev"
    use_fifos="yes"
    ;;
  *)
    dnl Unix OSS configuration

    dnl PortAudio for UNIX OSS
    portaudio_lib_other="-lm -lpthread ${forkpty}"
    use_fifos="no"
    ;;
esac

# use fifo instead of devnode (required on mac os x)
AM_CONDITIONAL(USE_FIFOS, test "$use_fifos" = "yes")


dnl look for Linux kernel source

AC_PATH_KERNEL_SOURCE

dnl here we see what drivers we could possibly even hope to run.
dnl this could be _much_ more sophisticated!  especially the lirc ones.

AC_MSG_CHECKING([for which drivers can be installed on this system])
AC_MSG_RESULT()

# this list must only contain userspace drivers without external
# (header/library) dependencies!

AH_TEMPLATE([PLUGINDIR],
        [The directory userspace drivers are loaded from])

AH_TEMPLATE([DEBUG],
        [Define to enable debugging output])

AH_TEMPLATE([DEVDIR],
        [device files directory])

AH_TEMPLATE([DYNCODES],
        [Define to use dynamic IR codes])

AH_TEMPLATE([HAVE_ALSA_SB_RC],
        [Define if the ALSA library with SB RC support is installed])

AH_TEMPLATE([HAVE_FORKPTY],
        [Define if forkpty is available])

AH_TEMPLATE([HAVE_IGUANAIR],
        [Define if the iguanaIR library is installed])

AH_TEMPLATE([HAVE_FTDI],
        [Define if the libftdi library is installed])

AH_TEMPLATE([HAVE_LIBALSA],
        [Define if the ALSA library is installed])

AH_TEMPLATE([HAVE_LIBCARACA],
        [Define if the caraca library is installed])

AH_TEMPLATE([HAVE_LIBIRMAN],
        [Define if the libirman library is installed])

AH_TEMPLATE([HAVE_LIBIRMAN_SW],
        [Define if the software only test version of libirman is installed])

AH_TEMPLATE([HAVE_LIBPORTAUDIO],
        [Define if the portaudio library is installed])

AH_TEMPLATE([HAVE_LIBUSB],
        [Define if libusb is installed])

AH_TEMPLATE([HAVE_LIBVGA],
        [Define if the complete vga libraries (vga, vgagl) are installed])

AH_TEMPLATE([HAVE_LINUX_DEVINPUT],
        [defined if Linux input interface is available])

AH_TEMPLATE([HAVE_LINUX_HIDDEV_FLAG_UREF],
        [defined if Linux hiddev HIDDEV_FLAG_UREF flag is available])

AH_TEMPLATE([HAVE_SCSI],
        [defined if SCSI API is available])

AH_TEMPLATE([HAVE_SOUNDCARD],
        [defined if soundcard API is available])

AH_TEMPLATE([HAVE_SYSTEMD],
        [defined if systemd API is available])

AH_TEMPLATE([HAVE_VSYSLOG],
        [define if you have vsyslog( prio, fmt, va_arg )])

AH_TEMPLATE([HW_DEFAULT],
        [The name of the hw_* structure to use by default])

AH_TEMPLATE([LIRC_DRIVER],
        [Text string signifying which driver is configured])

AH_TEMPLATE([LIRC_DRIVER_ANY],
        [Define to include most drivers])

AH_TEMPLATE([LIRC_HAVE_DEVFS],
        [Define if devfs support is present in current kernel])

AH_TEMPLATE([LIRC_IRQ],
        [Set the IRQ for the lirc driver])

AH_TEMPLATE([LIRC_IRTTY],
        [Set the default tty used by the drivers accessing /dev/ttyX])

AH_TEMPLATE([LIRC_SYSLOG],
        [syslog facility to use])

AH_TEMPLATE([LOCALSTATEDIR],
        [modifiable single-machine data])

AH_TEMPLATE([MAINTAINER_MODE],
        [define in maintainer mode])

AH_TEMPLATE([SYSCONFDIR],
        [system configuration directory])

AH_TEMPLATE([USE_SYSLOG],
        [define if you want to log to syslog instead of logfile])

AC_CHECK_LIB(caraca_client, caraca_init,[
  AM_CONDITIONAL([BUILD_CARACA],[true])
  ],[
  AM_CONDITIONAL([BUILD_CARACA],[false])]
)
AC_CHECK_HEADER(usb.h,[
  AC_DEFINE(HAVE_LIBUSB)
  AM_CONDITIONAL([BUILD_USB],[true])
  usb_libs=`${LIBUSB_CONFIG} --libs`
  ],[
  AM_CONDITIONAL([BUILD_USB],[false])]
)
AC_CHECK_HEADER(ftdi.h,[
  AM_CONDITIONAL([BUILD_FTDI],[true])
  ],[
  AM_CONDITIONAL([BUILD_FTDI],[false])]
)
AC_CHECK_HEADER(iguanaIR.h,[
  AM_CONDITIONAL([BUILD_IGUANAIR],[true])
  ],[
  AM_CONDITIONAL([BUILD_IGUANAIR],[false])]
)
AC_CHECK_LIB(irman, ir_strerror,[
  AM_CONDITIONAL([BUILD_LIBIRMAN],[true])
  ],[
  AM_CONDITIONAL([BUILD_LIBIRMAN],[false])]
)
AC_CHECK_LIB(irman_sw, ir_strerror,[
  AM_CONDITIONAL([BUILD_LIBIRMAN_SW],[true])
  ],[
  AM_CONDITIONAL([BUILD_LIBIRMAN_SW],[false])]
)
dnl audio driver requires PortAudio library installed and some linker flags
AC_CHECK_HEADERS(portaudio.h,[
  AC_CHECK_LIB(portaudio, Pa_Initialize,[
    AM_CONDITIONAL([BUILD_LIBPORTAUDIO],[true])
    ],[
    AM_CONDITIONAL([BUILD_LIBPORTAUDIO],[false])
    ],[
    ${portaudio_lib_other}]
  )
  ],[
  AM_CONDITIONAL([BUILD_LIBPORTAUDIO],[false])]
)
dnl audio_alsa driver requires ALSA library installed and some linker flags
have_alsa=no
AC_CHECK_HEADERS(alsa/asoundlib.h,[
  AC_CHECK_LIB(asound, snd_pcm_open,[
    AM_CONDITIONAL([BUILD_LIBALSA],[true])
    AC_DEFINE(HAVE_LIBALSA)
    have_alsa=yes
    ],[
    AM_CONDITIONAL([BUILD_LIBALSA],[false])],
    )
    ],[
    AM_CONDITIONAL([BUILD_LIBALSA],[false])]
)
dnl alsa_usb driver requires an ALSA that knows about our hwdep device
if test "$have_alsa" = "yes"; then
  AC_MSG_CHECKING(for ALSA SB RC hwdep support)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <alsa/asoundlib.h>
]], [[
  if (snd_hwdep_info_get_iface(NULL) == SND_HWDEP_IFACE_SB_RC) {}
]])],[
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_ALSA_SB_RC)
    AM_CONDITIONAL([BUILD_ALSA_SB_RC],[true])
    ],[
    AC_MSG_RESULT(no)
    AM_CONDITIONAL([BUILD_ALSA_SB_RC],[false])
  ])
fi
dnl creative_infracd driver requires linux scsi api
AC_CHECK_HEADERS(scsi/sg.h,[
  AC_DEFINE(HAVE_SCSI)
  ]
)
dnl dev_input driver requires linux input device api
AC_CHECK_HEADERS(linux/input.h,[
  AC_DEFINE(HAVE_LINUX_DEVINPUT)
  ]
)
dnl check if hiddev is available
AC_CHECK_HEADERS([linux/types.h])
AC_CHECK_HEADERS([linux/hiddev.h],[
  AC_MSG_CHECKING(for HIDDEV_FLAG_UREF support)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <linux/types.h>
#include <linux/hiddev.h>
    ]], [[
      int flag = HIDDEV_FLAG_UREF;
    ]])],[
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_LINUX_HIDDEV_FLAG_UREF)
    AM_CONDITIONAL([BUILD_HIDDEV],[true])
    ],[
    AC_MSG_RESULT(no)
    AM_CONDITIONAL([BUILD_HIDDEV],[false])
    ])
  ],[
  AM_CONDITIONAL([BUILD_HIDDEV],[false])
  ],
[#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
]
)
#dnl dsp driver requires linux soundcard api
AC_CHECK_HEADERS(sys/soundcard.h,[
  AM_CONDITIONAL([BUILD_DSP],[true])
  AC_DEFINE(HAVE_SOUNDCARD)
  ],[
  AM_CONDITIONAL([BUILD_DSP],[false])]
)
dnl i2cuser driver requires the Linux I2C API
AC_CHECK_HEADERS(linux/i2c-dev.h,[
  AM_CONDITIONAL([BUILD_I2CUSER],[true])
  ],[
  AM_CONDITIONAL([BUILD_I2CUSER],[false])]
)


PKG_CHECK_MODULES([SYSTEMD],[libsystemd-daemon],[AC_DEFINE(HAVE_SYSTEMD)],[true])
CFLAGS="$CFLAGS $SYSTEMD_CFLAGS"
LIBS="$LIBS $SYSTEMD_LIBS"

AC_CHECK_FILE([/usr/lib/systemd/system], [has_systemd_system=true],[true])
AM_CONDITIONAL([SYSTEMD_INSTALL],[test x"$has_systemd_system" = xtrue])

lirc_driver="userspace"

AC_MSG_NOTICE([Building all userspace drivers.])
lirc_driver="userspace"


dnl slightly more advanced options

dnl maintainer mode options

maintmode_daemons_extra=
AC_ARG_ENABLE(maintainer-mode,
[  --enable-maintainer-mode    enable maintainer specific things],
[
if test x${enableval} = xyes; then
AC_DEFINE(MAINTAINER_MODE)
maintmode_daemons_extra="lircd.simrec lircd.simsend slinke"
fi
])

dnl don't create nor destroy devices when installing and uninstalling

AC_ARG_ENABLE(sandboxed,
[  --enable-sandboxed      don't touch anything out of the install directory],
[ enable_sandboxed="${enableval}" ],
[ enable_sandboxed="no"])
AM_CONDITIONAL(SANDBOXED, test x$enable_sandboxed = xyes)

AC_ARG_ENABLE(install_etc,
[  --install-etc           Install /etc/lirc configuration files(default=yes)],
[ install_etc="${enableval}" ],
[ install_etc="yes"])
AM_CONDITIONAL(INSTALL_ETC, test x$install_etc = xyes)


AC_ARG_WITH(devdir,
[  --with-devdir=DIR       Install device files in DIR (/dev)],
devdir=${withval},
devdir=${devdir_default})

AC_ARG_ENABLE(dyncodes,
[  --enable-dyncodes       use dynamic codes],
test x${enableval} = xyes && AC_DEFINE(DYNCODES))

AC_ARG_ENABLE(debug,
[  --enable-debug          enable debugging features],
test x${enableval} = xyes && AC_DEFINE(DEBUG)
)

AC_ARG_WITH(syslog,
[  --with-syslog=facility  use syslog instead of logfile],
if test -n "${withval}"; then
  facility="${withval}"
  if test "${withval}" = yes; then
    facility=LOG_DAEMON;
  elif test "${withval}" = LOG_AUTH; then
    true;
  elif test "${withval}" = LOG_AUTHPRIV; then
    true;
  elif test "${withval}" = LOG_DAEMON; then
    true;
  elif test "${withval}" = LOG_LOCAL0; then
    true;
  elif test "${withval}" = LOG_LOCAL1; then
    true;
  elif test "${withval}" = LOG_LOCAL2; then
    true;
  elif test "${withval}" = LOG_LOCAL3; then
    true;
  elif test "${withval}" = LOG_LOCAL4; then
    true;
  elif test "${withval}" = LOG_LOCAL5; then
    true;
  elif test "${withval}" = LOG_LOCAL6; then
    true;
  elif test "${withval}" = LOG_LOCAL7; then
    true;
  elif test "${withval}" = LOG_USER; then
    true;
  else
    AC_MSG_ERROR([*** invalid facility (see syslog(3) for possible facilities) ])
  fi
  AC_DEFINE(USE_SYSLOG)
fi
)


receive=""
if ! echo ${hw_module}|grep " receive.o" >/dev/null; then
        receive="receive.o"
fi


dnl tell the Makefiles what we decided
AC_SUBST(daemon)
AC_SUBST(forkpty)
AC_SUBST(devdir)
AC_SUBST(sysconfdir)
AC_SUBST(driver)
AC_SUBST(lirc_driver)
AC_SUBST(vga_progs)
AC_SUBST(x_progs)
AC_SUBST(hw_module)
AC_SUBST(receive)
AC_SUBST(lircd_conf)
AC_SUBST(lircmd_conf)
AC_SUBST(maintmode_daemons_extra)
AC_SUBST(usb_libs)

dnl tell the C code what we decided
AC_DEFINE_UNQUOTED(DEVDIR, "$devdir")
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_DEFINE_UNQUOTED(PLUGINDIR, "$libdir/lirc/plugins",[
         Where userspace drivers are loaded from])
AC_DEFINE(LIRC_OPTIONS_VAR, ["LIRC_OPTIONS_PATH"],[
        Environment variable overriding options file path])
AC_DEFINE(LIRC_OPTIONS_PATH,["/etc/lirc/lirc_options.conf"],[
        Default path for lircd_options.conf])

if test "$irtty" != ""; then
  AC_DEFINE_UNQUOTED(LIRC_IRTTY, "$irtty")
fi
AC_DEFINE_UNQUOTED(LIRC_SYSLOG, $facility)

AH_TOP([
/* device file names - beneath DEVDIR (default /dev) */
#define DEV_LIRC        "lirc0"

/* daemon socket file names - beneath $varrundir (default /var/run/lirc) */
#define DEV_LIRCD       "lircd"
#define DEV_LIRCM       "lircm"

/* config file names - beneath SYSCONFDIR (default /etc) */
#define CFG_LIRCD       "lircd.conf"
#define CFG_LIRCM       "lircmd.conf"

/* config file names - beneath $HOME or SYSCONFDIR */
#define CFG_LIRCRC      "lircrc"

/* log files */
#define LOG_LIRCD       "lircd"
#define LOG_LIRMAND     "lirmand"

/* pid file */
#define PID_LIRCD       "lircd.pid"

/* default port number */
#define        LIRC_INET_PORT  8765
])

AH_BOTTOM([
#include      "paths.h"

#ifdef LIRC_HAVE_DEVFS
#define LIRC_DRIVER_DEVICE      DEVDIR "/" DEV_LIRC "/0"
#else
#define LIRC_DRIVER_DEVICE      DEVDIR "/" DEV_LIRC
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
])

dnl bug in automake/autoconf?
dnl SHELL=/bin/sh AC_SUBST(SHELL)

dnl write these decisions out to the Makefiles
AC_CONFIG_FILES([
  Makefile
  lirc.pc
  lirc-driver.pc
  daemons/Makefile
  plugins/Makefile
  tools/Makefile
  lib/Makefile
  doc/Makefile
  systemd/Makefile
])
AC_OUTPUT

echo
echo "Now enter 'make' and 'make install' to compile and install the package."
echo

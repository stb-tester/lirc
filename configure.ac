
dnl Process this file with autoconf to produce a configure script.
dnl configure.ac, (c)1999 Tom Wheeley <tw104@york.ac.uk>

AC_INIT([lirc], [0.9.2a])
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
AC_PATH_PROG(depmod, depmod, /sbin/depmod, $PATH:/sbin)
AC_CHECK_PROG([PKGCONFIG],[pkg-config],[yes],[no])
if test x$PKGCONFIG = xno; then
  AC_MSG_ERROR([Required program pkg-config is missing])
fi
AC_CHECK_PROG([MAN2HTML],[man2html],[yes],[no])
AM_CONDITIONAL(HAVE_MAN2HTML, test x$MAN2HTML = xyes)
AC_CHECK_PROG([DOXYGEN],[doxygen],[yes],[no])
AM_CONDITIONAL(HAVE_DOXYGEN, test x$DOXYGEN = xyes)
AC_PROG_LN_S
LT_INIT([disable-static])
AM_PATH_PYTHON([3.1],,[:])
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
AX_PYTHON_MODULE([yaml])
AM_CONDITIONAL([HAVE_PYMOD_YAML], test $HAVE_PYMOD_YAML = "yes")
forkpty=""
AC_CHECK_FUNCS(forkpty)
if test "$ac_cv_func_forkpty" != yes; then
  AC_CHECK_LIB(util,forkpty,[
    forkpty="-lutil"
    AC_DEFINE(HAVE_FORKPTY)])
fi

lockdir_default="/var/lock/lockdev"
test -d "/var/lock/lockdev" || lockdir_default="/var/lock"

AM_CONDITIONAL([DARWIN], [case $host_os in darwin*) true;; *) false;; esac])

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
    portaudio_lib_other="-framework CoreAudio -framework AudioToolbox"
    ;;
  *)
    dnl Unix OSS configuration
    portaudio_lib_other="-lm -lpthread ${forkpty}"
    ;;
esac

# use fifo instead of devnode (required on mac os x)
AM_CONDITIONAL(USE_FIFOS, test "$use_fifos" = "yes")

AC_MSG_CHECKING([for which drivers can be installed on this system])
AC_MSG_RESULT()

# this list must only contain userspace drivers without external
# (header/library) dependencies!

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

AH_TEMPLATE([LIRC_HAVE_DEVFS],
        [Define if devfs support is present in current kernel])

AH_TEMPLATE([LIRC_LOCKDIR],
        [Directory for device locks (usually /var/lock or /var/lock/lockdev)])

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
PKG_CHECK_MODULES([FTDI],[libftdi],,[true])
test -z "$FTDI_LIBS" && PKG_CHECK_MODULES([FTDI], [libftdi1],,[true])
if test -n "$FTDI_LIBS"; then
  CFLAGS="$CFLAGS $FTDI_CFLAGS"
  LDFLAGS="$LDFLAGS $FTDI_LIBS"
  AC_DEFINE(HAVE_FTDI)
  AM_CONDITIONAL([BUILD_FTDI],[true])
  AC_SUBST(FTDI_LIBS)
else
  AM_CONDITIONAL([BUILD_FTDI],[false])]
fi
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
else
  AM_CONDITIONAL([BUILD_ALSA_SB_RC],[false])
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


PKG_CHECK_MODULES([SYSTEMD],[libsystemd-daemon],,[true])
if test -n "$SYSTEMD_LIBS"; then
    CFLAGS="$CFLAGS $SYSTEMD_CFLAGS"
    AC_DEFINE(HAVE_SYSTEMD)
fi
SYSTEMD_SYSTEMUNITDIR

AC_MSG_NOTICE([Building all userspace drivers.])

AC_ARG_WITH(lockdir,
  [  --with-lockdir=DIR      Old-school device lock files in DIR (/var/lock)],
  lockdir=${withval},
  lockdir=${lockdir_default})
AC_DEFINE_UNQUOTED(LIRC_LOCKDIR, "$lockdir")

AC_ARG_ENABLE(install_etc,
[  --install-etc           Install /etc/lirc configuration files(default=yes)],
[ install_etc="${enableval}" ],
[ install_etc="yes"])
AM_CONDITIONAL(INSTALL_ETC, test x$install_etc = xyes)

AC_ARG_ENABLE([python3_fix],
  [  --enable-python3-fix    Install python3 symlink to python 3.x (macOS fix)],
  [],
  [enable_python3_fix="no"])
AM_CONDITIONAL(NEED_PYTHON3, [test "x$enable_python3_fix" = "xyes"])

dnl tell the Makefiles what we decided
AC_SUBST(daemon)
AC_SUBST(forkpty)
AC_SUBST(sysconfdir)
AC_SUBST(usb_libs)
AC_SUBST(x_progs)

dnl tell the C code what we decided
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_DEFINE(LIRC_OPTIONS_VAR, ["LIRC_OPTIONS_PATH"],[
        Environment variable overriding options file path])

AH_TOP([
#include     "paths.h"
])

AH_BOTTOM([
#include     "lirc_config.h"
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
AC_DEFUN([AC_REPORT_CONDITIONAL], [
AS_ECHO_N([$1:])
AS_IF(test -z $$1_TRUE,AS_ECHO([yes]),AS_ECHO([no]))])

AC_MSG_RESULT([
Summary of selected options:
----------------------------------------
prefix:                         $prefix
sysconfdir:                     $sysconfdir
x_progs:                        $x_progs
host:                           $host
host_os:                        $host_os
forkpty:                        $forkpty
usb_libs                        $usb_libs
lockdir:                        $lockdir

Conditionals:
])

AC_REPORT_CONDITIONAL([BUILD_ALSA_SB_RC])
AC_REPORT_CONDITIONAL([BUILD_HIDDEV])
AC_REPORT_CONDITIONAL([BUILD_LIBALSA])
AC_REPORT_CONDITIONAL([BUILD_LIBPORTAUDIO])
AC_REPORT_CONDITIONAL([BUILD_CARACA])
AC_REPORT_CONDITIONAL([BUILD_DSP])
AC_REPORT_CONDITIONAL([BUILD_FTDI])
AC_REPORT_CONDITIONAL([BUILD_HIDDEV])
AC_REPORT_CONDITIONAL([BUILD_I2CUSER])
AC_REPORT_CONDITIONAL([BUILD_IGUANAIR])
AC_REPORT_CONDITIONAL([BUILD_LIBIRMAN])
AC_REPORT_CONDITIONAL([BUILD_LIBIRMAN_SW])
AC_REPORT_CONDITIONAL([BUILD_LIBPORTAUDIO])
AC_REPORT_CONDITIONAL([BUILD_USB])
AC_REPORT_CONDITIONAL([INSTALL_ETC])
AC_REPORT_CONDITIONAL([USE_FIFOS])
AC_REPORT_CONDITIONAL([BUILD_XTOOLS])
AC_REPORT_CONDITIONAL([HAVE_PYTHON])
AC_REPORT_CONDITIONAL([NEED_PYTHON3])
AC_REPORT_CONDITIONAL([SYSTEMD_INSTALL])
AC_REPORT_CONDITIONAL([HAVE_MAN2HTML])
AC_REPORT_CONDITIONAL([HAVE_DOXYGEN])
AC_REPORT_CONDITIONAL([HAVE_PYMOD_YAML])

if test "$HAVE_PYMOD_YAML" != "yes"; then
    AC_MSG_WARN([python3-yaml is missing. lirc-setup will not work, table.html
and lirc.hwdb will not be created.])
fi

echo
echo "Now enter 'make' and 'make install' to compile and install the package."
echo

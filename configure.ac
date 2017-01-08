
dnl Process this file with autoconf to produce a configure script.
dnl configure.ac, (c)1999 Tom Wheeley <tw104@york.ac.uk>

AC_INIT([lirc], [0.9.4d])
AC_CONFIG_SRCDIR([daemons/lircd.cpp])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(SH_PATH,[sh])
AC_CHECK_PROGS([MODINFO], [modinfo], [no], [$PATH:/sbin:/usr/sbin])
if test x$MODINFO = xno; then
  AC_MSG_WARN(["No modinfo command found - skipping kernel drivers."])
  MODINFO="false"
else
  MODINFO=$( PATH=$PATH:/sbin:/usr/sbin which modinfo )
fi
AC_SUBST(MODINFO)
AC_CHECK_PROG([PKGCONFIG],[pkg-config],[yes],[no])
if test x$PKGCONFIG = xno; then
  AC_MSG_ERROR([Required program pkg-config is missing or not in \$PATH])
fi
AC_CHECK_PROG([XSLTPROC],[xsltproc],[yes],[no])
if test x$XSLTPROC != "xyes"; then
    AC_MSG_ERROR([
      xsltproc is required to build. It usually comes with libxml])
fi
AM_CONDITIONAL(HAVE_UINPUT, test -n "$HAVE_UINPUT" -o -e /dev/uinput)
AC_CHECK_PROG([MAN2HTML],[man2html],[yes],[no])
AM_CONDITIONAL(HAVE_MAN2HTML, test x$MAN2HTML = xyes)
AC_CHECK_PROG([DOXYGEN],[doxygen],[yes],[no])
AM_CONDITIONAL(HAVE_DOXYGEN, test x$DOXYGEN = xyes)
AC_PROG_LN_S
LT_INIT([disable-static])
AM_PATH_PYTHON([3.1],,)
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON" != ""])
AC_CHECK_PROG([HAVE_PYTHON3],[python3],[yes],[no])
LIBUSB_CONFIG="pkg-config libusb"

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h libudev.h limits.h linux/ioctl.h linux/sched.h \
		  poll.h sys/ioctl.h  sys/poll.h sys/time.h syslog.h  \
		  unistd.h util.h libutil.h pty.h])

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
AC_SEARCH_LIBS([dlopen], [dl dld], [], [
  AC_MSG_ERROR([unable to find the dlopen() function])
])
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

case $host_os in
    darwin*) osflags='-DDARWIN'
	;;
    *bsd*) osflags='-DFREEBSD'
	;;
    *)  osflags=''
esac
AC_SUBST(osflags)

AM_CONDITIONAL([DARWIN], [test "$osflags" = "-DDARWIN"])

AC_MSG_CHECKING([for linux kernel])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ ]], [[
    #ifndef __linux__
    #error Not linux
    #endif
  ]])],[
    AC_MSG_RESULT(yes)
    AM_CONDITIONAL([LINUX_KERNEL],[true])
  ],[
    AC_MSG_RESULT(no)
    AM_CONDITIONAL([LINUX_KERNEL],[false])
  ]
)


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

AC_MSG_CHECKING([for which drivers can be installed on this system])
AC_MSG_RESULT()

# this list must only contain userspace drivers without external
# (header/library) dependencies!

AH_TEMPLATE([HAVE_ALSA_SB_RC],
        [Define if the ALSA library with SB RC support is installed])

AH_TEMPLATE([HAVE_FORKPTY],
        [Define if forkpty is available])

AH_TEMPLATE([HAVE_FTDI],
        [Define if the libftdi library is installed])

AH_TEMPLATE([HAVE_LIBALSA],
        [Define if the ALSA library is installed])

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

AH_TEMPLATE([HAVE_POLL_FINE],
        [defined  if poll() is available and seemingly without bugs])

AH_TEMPLATE([HAVE_POLL_H],
        [defined  if poll.h is available])

AH_TEMPLATE([HAVE_PTY_H],
        [defined  if pty.h is available])

AH_TEMPLATE([HAVE_SYS_POLL_H],
        [defined  if sys/poll.h is available])

AH_TEMPLATE([HAVE_LINUX_SCHED_H],
        [defined  if linux/sched.h is available])

AH_TEMPLATE([HAVE_SCSI],
        [defined if SCSI API is available])

AH_TEMPLATE([HAVE_SELECT],
        [defined if select(2) is available])

AH_TEMPLATE([HAVE_SOUNDCARD],
        [defined if soundcard API is available])

AH_TEMPLATE([HAVE_SYSTEMD],
        [defined if systemd API is available])

AH_TEMPLATE([HAVE_VSYSLOG],
        [define if you have vsyslog( prio, fmt, va_arg )])

AH_TEMPLATE([HAVE_KERNEL_LIRC_H],
        [define if kernel-headers contains lirc.]h)

AH_TEMPLATE([LIRC_HAVE_DEVFS],
        [Define if devfs support is present in current kernel])

AH_TEMPLATE([LIRC_LOCKDIR],
        [Directory for device locks (usually /var/lock or /var/lock/lockdev)])

AH_TEMPLATE([SH_PATH], [Path to shell, usually /bin/sh or /usr/bin/sh])

AH_TEMPLATE([HAVE_INT_GETGROUPLIST_GROUPS],
        [Define if getgrouplist() groups arg is int*])

AH_TEMPLATE([HAVE_LIBUDEV_H],
        [Define if libudev is installed])

AH_TEMPLATE([HAVE_UTIL_H],
        [Define if util.h is installed])

AH_TEMPLATE([HAVE_USB_H],
        [Define if usb.h is installed])

AH_TEMPLATE([HAVE_LIBUSB_H],
        [Define if libusb.h is installed])

AH_TEMPLATE([HAVE_LIBUSB_1_0_LIBUSB_H],
        [Define if libusb-1.0/libusb.h is installed])

AC_CHECK_HEADERS([usb.h libusb-1.0/libusb.h libusb.h])
AC_CHECK_LIB([usb],[usb_get_string],[usb_libs="-lusb"],,)
AM_CONDITIONAL([BUILD_USB],[test x$usb_libs != x])
AC_CHECK_LIB([usb-1.0],[libusb_lock_events],[usb_libs="$usb_libs -lusb-1.0"],,)


PKG_CHECK_MODULES([FTDI],[libftdi >= 1.0],,[true])
test -z "$FTDI_LIBS" && PKG_CHECK_MODULES([FTDI], [libftdi1 >= 1.0],,[true])
if test -n "$FTDI_LIBS"; then
  AC_DEFINE(HAVE_FTDI)
  AM_CONDITIONAL([BUILD_FTDI],[true])
  AC_SUBST(FTDI_LIBS)
  AC_SUBST(FTDI_CFLAGS)
else
  AM_CONDITIONAL([BUILD_FTDI],[false])
fi

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

dnl see https://daniel.haxx.se/blog/2016/10/11/poll-on-mac-10-12-is-broken/
AC_MSG_CHECKING(for working poll(2))
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
  #include <errno.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <stdio.h>
  #include <sys/time.h>
]],[[
  int fd = open("/dev/tty", O_RDONLY);
  if (fd == -1) return 2;
  struct pollfd pfd = {fd, 1, 0};

  struct timeval before, after;
  gettimeofday(&before, NULL);
  int rc = poll(&pfd, 1, 500);
  if (rc < 0) return errno;
  if (rc > 0) return rc;
  gettimeofday(&after, NULL);

  suseconds_t us = (after.tv_sec - before.tv_sec) * 1000000 +
    (after.tv_usec - before.tv_usec);
  return us >= 400000 ? 0 : 1;
]])],[
  AC_DEFINE(HAVE_POLL_FINE)
  AC_MSG_RESULT(yes)
],[
  AC_MSG_RESULT(no)
],[
  case x$HAVE_WORKING_POLL in
    xyes)
      AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_POLL_FINE)
      ;;
    xno)
      AC_MSG_RESULT(no)
      ;;
    *) AC_MSG_ERROR([
         Define HAVE_WORKING_POLL to "yes" or "no" when cross-compiling])
      ;;
  esac
])


dnl check for kernel-headers
AC_CHECK_HEADERS([linux/input.h],
                 [:],
                 [AC_MSG_WARN([Cannot find kernel headers])])

dnl check if kernel-headers has lirc.h
AC_CHECK_HEADERS([linux/lirc.h], [AC_DEFINE(HAVE_KERNEL_LIRC_H)])

dnl creative_infracd driver requires linux scsi api
AC_CHECK_HEADERS(scsi/sg.h,[ AC_DEFINE(HAVE_SCSI) ])

dnl dev_input driver requires linux input device api
AC_CHECK_HEADERS(linux/input.h,[ AC_DEFINE(HAVE_LINUX_DEVINPUT) ])

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

dnl Several plugins uses libudev to enumerate devices
AC_CHECK_HEADERS(libudev.h,[
  AM_CONDITIONAL([HAVE_LIBUDEV],[true])
  ],[
  AM_CONDITIONAL([HAVE_LIBUDEV],[false])]
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

PKG_CHECK_MODULES([LIBUDEV],[libudev],,[true])
CFLAGS="$CFLAGS $LIBUDEV_CFLAGS"
LIBS="$LIBS $LIBUDEV_LIBS"

dnl Ubuntu's systemd pkg-config seems broken beyond repair. So:
kernelversion=`cat /proc/version || echo "non-linux"`
case $kernelversion in
   *Ubuntu*)
        AC_MSG_NOTICE([Hardwiring Ubuntu systemd setup])
        AC_SUBST([systemdsystemunitdir], [/lib/systemd/system])
        AM_CONDITIONAL([WITH_SYSTEMDSYSTEMUNITDIR], [true])
        ;;
    *)
        SYSTEMD_SYSTEMUNITDIR
        ;;
esac


saved_cflags="$CFLAGS"
CFLAGS="-Werror -Wall"
AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([
    #include <grp.h>
    #include <unistd.h>
    int main(int argc, char**argv)
    {
        int groups[[4]];
        int ngroups;
        getgrouplist("foo", (gid_t)1, groups, &ngroups);
    }
    ])
  ],
  [AC_DEFINE(HAVE_INT_GETGROUPLIST_GROUPS)],
  [true]
)
CFLAGS="$saved_cflags"

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
  [  --enable-python3-fix    Install python3 wrapper to python 3.x],
  [],
  [enable_python3_fix="no"])
AM_CONDITIONAL(NEED_PYTHON3, [test "x$enable_python3_fix" = "xyes"])

AC_ARG_VAR(HAVE_WORKING_POLL,[
	   On cross-compile: Target has a working poll(2) implementation])

AC_ARG_VAR(HAVE_UINPUT,[
  If not-empty, assume uinput is available despite missing /dev/uinput
])


suffix=$(echo "$VERSION" | sed 's/.*-//')
AM_CONDITIONAL(DEVEL, [test x$suffix = xdevel])

dnl tell the Makefiles what we decided
AC_SUBST(daemon)
AC_SUBST(forkpty)
AC_SUBST(sysconfdir)
AC_SUBST(usb_libs)
AC_SUBST(x_progs)

dnl tell the C code what we decided
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_DEFINE_UNQUOTED(SH_PATH, "$SH_PATH")
AC_DEFINE(LIRC_OPTIONS_VAR, ["LIRC_OPTIONS_PATH"],[
        Environment variable overriding options file path])

AH_TOP([
#include     "paths.h"
])

AH_BOTTOM([
#include     "lirc_config.h"
])

dnl write these decisions out to the Makefiles
AC_CONFIG_FILES([
  VERSION
  Makefile
  lirc.pc
  lirc-driver.pc
  daemons/Makefile
  plugins/Makefile
  tools/Makefile
  lib/Makefile
  doc/Makefile
  configs/Makefile
  doc/docpage.xsl
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
AC_REPORT_CONDITIONAL([BUILD_DSP])
AC_REPORT_CONDITIONAL([BUILD_FTDI])
AC_REPORT_CONDITIONAL([BUILD_HIDDEV])
AC_REPORT_CONDITIONAL([BUILD_I2CUSER])
AC_REPORT_CONDITIONAL([BUILD_LIBALSA])
AC_REPORT_CONDITIONAL([BUILD_LIBPORTAUDIO])
AC_REPORT_CONDITIONAL([BUILD_USB])
AC_REPORT_CONDITIONAL([BUILD_XTOOLS])
AC_REPORT_CONDITIONAL([HAVE_DOXYGEN])
AC_REPORT_CONDITIONAL([HAVE_LIBUDEV])
AC_REPORT_CONDITIONAL([HAVE_MAN2HTML])
AC_REPORT_CONDITIONAL([HAVE_PYMOD_YAML])
AC_REPORT_CONDITIONAL([INSTALL_ETC])
AC_REPORT_CONDITIONAL([NEED_PYTHON3])
AC_REPORT_CONDITIONAL([SYSTEMD_INSTALL])
AC_REPORT_CONDITIONAL([DEVEL])
AC_REPORT_CONDITIONAL([HAVE_UINPUT])
AC_REPORT_CONDITIONAL([DARWIN])
AC_REPORT_CONDITIONAL([LINUX_KERNEL])

echo

if test "$HAVE_PYMOD_YAML" != "yes"; then
    AC_MSG_WARN([python3-yaml is missing. lirc-setup will not work, table.html
and lirc.hwdb will not be created.])
fi
if test "$HAVE_PYTHON3" = "no" -a x$enable_python3_fix != "x$yes"; then
    AC_MSG_WARN([
python3 link is missing, re-configure using --enable-python3-fix or just
make a symlink])
fi

echo
echo "Now enter 'make' and 'make install' to compile and install the package."
echo

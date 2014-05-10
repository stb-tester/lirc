##
## additional m4 macros
##
## (C) 1999 Christoph Bartelmus (lirc@bartelmus.de)
##


dnl check for kernel source

AC_DEFUN([AC_PATH_KERNEL_SOURCE_SEARCH],
[
  kerneldir=missing
  kernelext=ko
  no_kernel=yes

  if test `uname` != "Linux"; then
    kerneldir="not running Linux"
  else
    for dir in ${ac_kerneldir} \
        /usr/src/kernel-source-* \
        /usr/src/linux-source-* \
        /usr/src/linux /lib/modules/*/source \
        /lib/modules/*/build
    do
      if test -d $dir; then
        kerneldir=`dirname $dir/Makefile`/ || continue
        no_kernel=no
        break
      fi;
    done
  fi

  if test x${no_kernel} = xyes; then
     ac_cv_have_kernel="no_kernel=yes kerneldir=srcdir kernelext=ko"

  else
     ac_cv_have_kernel="no_kernel=no kerneldir=${kerneldir} kernelext=ko"
  fi
]
)

AC_DEFUN([AC_PATH_KERNEL_SOURCE],
[
  AC_CHECK_PROG(ac_pkss_mktemp,mktemp,yes,no)
  AC_PROVIDE([AC_PATH_KERNEL_SOURCE])
  AC_MSG_CHECKING(for Linux kernel sources)

  AC_ARG_WITH(kerneldir,
    [  --with-kerneldir=DIR    kernel sources in DIR],

    ac_kerneldir=${withval}
    AC_PATH_KERNEL_SOURCE_SEARCH,

    ac_kerneldir=""
    AC_CACHE_VAL(ac_cv_have_kernel,AC_PATH_KERNEL_SOURCE_SEARCH)
  )
  eval "$ac_cv_have_kernel"
  if test "$kerneldir" = "srcdir"; then
    kerneldir='$(top_srcdir)'
  fi
  AC_SUBST(kerneldir)
  AC_SUBST(kernelext)
  AC_MSG_RESULT(${kerneldir})
]
)

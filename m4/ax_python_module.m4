# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_python_module.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_PYTHON_MODULE(modname[, fatal])
#
# DESCRIPTION
#
#   Checks for Python module.
#
#   If fatal is non-empty then absence of a module will trigger an error.
#
# LICENSE
#
#   Copyright (c) 2008 Andrew Collier
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 6

AU_ALIAS([AC_PYTHON_MODULE], [AX_PYTHON_MODULE])
AC_DEFUN([AX_PYTHON_MODULE],[
    PYTHON=${PYTHON:-"python3"}
    AC_MSG_CHECKING([$(basename $PYTHON) module: $1])
    AS_IF([$PYTHON -c "import $1" 2>/dev/null], [
        AC_MSG_RESULT(yes)
        AS_TR_CPP(HAVE_PYMOD_$1)='yes'
    ], [
        AC_MSG_RESULT(no)
        eval AS_TR_CPP(HAVE_PYMOD_$1)='no'
        AS_IF([test x$2 != x],[
            AC_MSG_ERROR([failed to find required python module $1])
            exit 1
        ])
    ])
])

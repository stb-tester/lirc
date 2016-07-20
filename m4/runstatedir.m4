AC_DEFUN([DEFINE_RUNSTATEDIR],
[
 dnl Added in autoconf 2.70
 if test "x$runstatedir" = x; then
 AC_SUBST([runstatedir], ['${localstatedir}/run'])
 fi
])

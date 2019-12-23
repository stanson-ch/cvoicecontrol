dnl Process this file with autoconf to produce a configure script.

AC_INIT([cvoicecontrol], [0.9.1])
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

AC_SUBST([backend],[oss])

dnl Checks for libraries.
AC_CHECK_LIB([m],[main],[],[AC_MSG_ERROR([unable to find libm])])
AC_CHECK_LIB([pthread],[main],[],[AC_MSG_ERROR([unable to find libpthread])])
AC_CHECK_LIB([ncurses],[main],[],[AC_MSG_ERROR([unable to find libncurses])])
AC_CHECK_LIB([asound],[main])

AS_IF([test "x$ac_cv_lib_asound_main" = xyes],[AC_SUBST([backend],[alsa])])

dnl Checks for header files.
AC_CHECK_HEADERS(fcntl.h glob.h math.h ncurses.h pthread.h signal.h stdio.h stdlib.h string.h sys/ioctl.h sys/select.h sys/soundcard.h sys/time.h sys/types.h termios.h time.h unistd.h)
dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

AC_OUTPUT(Makefile \
src/Makefile \
docs/Makefile \
docs/en/Makefile)

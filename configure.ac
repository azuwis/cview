#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT(CView, 0.1, http://azuwis.github.com/)
AC_CONFIG_SRCDIR([src/cview.c])
AC_CONFIG_HEADERS([src/config.h])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.
PKG_CHECK_MODULES(DEPS, gthread-2.0 gio-2.0 >= 2.17.2 gtk+-2.0 gtkimageview >= 1.3)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([strcasecmp])

AC_CONFIG_FILES([Makefile
		 src/Makefile])
AC_OUTPUT

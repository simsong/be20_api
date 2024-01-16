#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Order is largely irrelevant, although it must start with AC_INIT and end with AC_OUTPUT
# See http://autotoolset.sourceforge.net/tutorial.html
# and http://www.openismus.com/documents/linux/automake/automake.shtml
# This is used solely for creating the test program

# Review Makefile.am which includes Makefile.defs

#
# This file is public domain
#

AC_PREREQ([2.69])
AC_INIT([BE20_API],[2.1.0],[bugs@digitalcorpora.org])
AC_CONFIG_MACRO_DIR(m4)

AC_MSG_NOTICE([at start CPPFLAGS are $CPPFLAGS])


AC_CONFIG_FILES([Makefile ])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_PROG_RANLIB
AM_PROG_AR

# Programs we will be using
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O                    dnl allow per-produce flags
AC_PROG_INSTALL

# Must use C++17 mode.
m4_include([m4/ax_cxx_compile_stdcxx.m4])
AC_LANG_PUSH(C++)
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AC_LANG_POP()

# Turn on all warnings
m4_include([m4/slg_gcc_all_warnings.m4])


################################################################
# Specify our headers

## Check for headers used by bulk Extractor
## do not put pthread here

## Check dfxml and be20_api headers

m4_include([dfxml_cpp/src/dfxml_configure.m4])
m4_include([m4/slg_address_sanitizer.m4])
m4_include([m4/slg_noopt.m4])
m4_include([be20_configure.m4])
AC_MSG_NOTICE([hello point 1])

AC_MSG_NOTICE([CFLAGS are now $CFLAGS])
AC_MSG_NOTICE([hello point 2])
AC_MSG_NOTICE([CPPFLAGS are now $CPPFLAGS])
AC_MSG_NOTICE([CXXFLAGS are now $CXXFLAGS])


################################################################
# Take out duplicate flags
CFLAGS=$(echo $CFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')
CPPFLAGS=$(echo $CPPFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')
CXXFLAGS=$(echo $CXXFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')

############## drop optimization flags and add -g if requested ################
if test "${with_opt}" = "no" ; then
  CFLAGS=`echo   -g "$CFLAGS"   | sed s/-O[[0-9]]//`             # note the double quoting!
  CXXFLAGS=`echo -g "$CXXFLAGS" | sed s/-O[[0-9]]//`
fi

LIBS="-lstdc++ $LIBS"

AC_MSG_NOTICE([CFLAGS are now $CFLAGS])
AC_MSG_NOTICE([CPPFLAGS are now $CPPFLAGS])
AC_MSG_NOTICE([CXXFLAGS are now $CXXFLAGS])


AC_OUTPUT

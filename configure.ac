#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# Order is largely irrelevant, although it must start with AC_INIT and end with AC_OUTPUT
# See http://autotoolset.sourceforge.net/tutorial.html
# and http://www.openismus.com/documents/linux/automake/automake.shtml

#
# This file is public domain
#

AC_PREREQ([2.71])
AC_INIT([BE13_API],[1.0.0],[bugs@digitalcorpora.org])
AC_CONFIG_MACRO_DIR(m4)

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

# Must use C++17 mode. (mandatory)
m4_include([m4/ax_cxx_compile_stdcxx.m4])
AC_LANG_PUSH(C++)
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AC_LANG_POP()

# Turn on all warnings
m4_include([m4/slg_gcc_all_warnings.m4])

# Extra files
AC_REQUIRE_AUX_FILE([foobar.txt])


################################################################
# Specify our headers

# Autoupdate added the next two lines to ensure that your configure
# script's behavior did not change.  They are probably safe to remove.
AC_CHECK_INCLUDES_DEFAULT
AC_PROG_EGREP

## Check for headers used by bulk Extractor
## do not put pthread here

## Check dfxml and be13_api headers
m4_include([dfxml/src/dfxml_configure.m4])
m4_include([be13_configure.m4])

AC_OUTPUT

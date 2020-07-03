#                                               -*- Autoconf -*- 
# Process this file with autoconf to produce a configure script. 
# Order is largely irrelevant, although it must start with AC_INIT and end with AC_OUTPUT 
# See http://autotoolset.sourceforge.net/tutorial.html 
# and http://www.openismus.com/documents/linux/automake/automake.shtml 

#
# This file is public domain
#

AC_PREREQ(2.57)
AC_INIT(BE13_API, 1.0.0, bugs@digitalcorpora.org)
AC_CONFIG_MACRO_DIR(m4)

# Load in additional macros
m4_include([ax_cxx_compile_stdcxx.m4])
m4_include([ax_cxx_compile_stdcxx_14.m4])


AC_CONFIG_FILES([Makefile ])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_PROG_RANLIB
AM_PROG_AR

AC_MSG_NOTICE([point 1])

# Programs we will be using
AC_PROG_CC
AC_MSG_NOTICE([point 2])
AC_PROG_CXX
AC_MSG_NOTICE([point 3])
AM_PROG_CC_C_O                    dnl allow per-produce flags
AC_MSG_NOTICE([point 4])
AC_PROG_INSTALL

AC_MSG_NOTICE([point 5])

# Must use C++14 mode. (mandatory)
# Don't use the GNU C++11 extensions for portability (noext).
AC_MSG_NOTICE([point 6])

AC_MSG_NOTICE([point 6])

AC_LANG_PUSH(C++)
AX_CXX_COMPILE_STDCXX_14(noext, mandatory)
AC_LANG_POP()
AC_MSG_NOTICE([point 7])

# We must use C14 and C++14
#CFLAGS="$CFLAGS -std=c14"
#CXXFLAGS="$CXXFLAGS -std=c++14"

# Turn on all warnings
m4_include([m4/slg_gcc_all_warnings.m4])

# Check for warnings that cester should disable
m4_include([m4/cester_configure.m4])


################################################################
# Specify our headers

AC_HEADER_STDC
## Check for headers used by bulk Extractor
## do not put pthread here
AC_CHECK_HEADERS([alloca.h dirent.h dlfcn.h err.h errno.h fcntl.h inttypes.h libgen.h limits.h malloc.h mmap.h pwd.h signal.h stdarg.h stdint.h stdio.h strings.h string.h stdlib.h sys/cdefs.h sys/disk.h sys/fcntl.h sys/ioctl.h sys/mman.h sys/mmap.h sys/mount.h sys/param.h sys/socket.h sys/stat.h sys/types.h sys/time.h sys/resource.h time.h unistd.h windows.h CoreServices/CoreServices.h])

## Check dfxml and be13_api headers
m4_include([dfxml/src/dfxml_configure.m4])
m4_include([be13_configure.m4])


AC_OUTPUT

AC_PREREQ([2.71])
AC_INIT([BE20_API],[2.1.0],[bugs@digitalcorpora.org])
AC_CONFIG_SRCDIR([Makefile.am])        dnl or src/main.c / src/...
AC_CONFIG_AUX_DIR([build-aux])
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE

m4_include([be20_configure.m4])
m4_include([dfxml_cpp/src/dfxml_configure.m4])
m4_include([m4/slg_address_sanitizer.m4])
m4_include([m4/slg_noopt.m4])

AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile ])

dnl Enforce C++20
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])

dnl Optional: pkg-config and deps
PKG_PROG_PKG_CONFIG
dnl PKG_CHECK_MODULES([DEPS], [foo >= 1.2 bar])

dnl Optional feature toggles (asan example)
AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--enable-asan], [Build with AddressSanitizer])],
  [], [enable_asan=no])
AS_IF([test "x$enable_asan" = "xyes"], [
  dnl Append rather than overwrite; keep user flags intact
  CXXFLAGS="$CXXFLAGS -fsanitize=address -fno-omit-frame-pointer"
  LDFLAGS="$LDFLAGS -fsanitize=address"
])


################################################################
# Take out duplicate flags
CFLAGS=$(echo $CFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')
CPPFLAGS=$(echo $CPPFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')
CXXFLAGS=$(echo $CXXFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')
AM_LDFLAGS=$(echo $LDFLAGS | tr ' ' '\n' | sort -u | tr '\n' ' ')

AC_MSG_NOTICE([*************************************])
AC_MSG_NOTICE([*************************************])
AC_MSG_NOTICE([  PACKAGE_NAME:     $PACKAGE_NAME])
AC_MSG_NOTICE([  PACKAGE_VERSION:  $PACKAGE_VERSION])
AC_MSG_NOTICE([  CC:               $CC])
AC_MSG_NOTICE([  CXX:              $CXX])
AC_MSG_NOTICE([  CPPFLAGS:         $CPPFLAGS])
AC_MSG_NOTICE([  CFLAGS:           $CFLAGS])
AC_MSG_NOTICE([  CXXFLAGS:         $CXXFLAGS])
AC_MSG_NOTICE([  LIBS:             $LIBS])
AC_MSG_NOTICE([  LDFLAGS:          $LDFLAGS])


AC_OUTPUT

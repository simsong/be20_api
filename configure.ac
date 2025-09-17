AC_PREREQ([2.71])
AC_INIT([BE20_API],[2.1.0],[bugs@digitalcorpora.org])
AC_CONFIG_SRCDIR([Makefile.am])        dnl or src/main.c / src/...
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror=foreign])
AC_USE_SYSTEM_EXTENSIONS

AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX

dnl If you want to enforce a standard, prefer this over rewriting $CXX:
dnl AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])

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

AC_CONFIG_HEADERS([config.h])

dnl List every Makefile you expect Automake to generate
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  tests/Makefile
])
AC_OUTPUT

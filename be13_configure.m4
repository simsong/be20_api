#
# mix-ins for be13
#

AC_MSG_NOTICE([Including be13_configure.m4 from be13_api])
AC_CHECK_HEADERS([err.h pwd.h sys/cdefs.h sys/mman.h sys/resource.h sys/utsname.h unistd.h])
AC_CHECK_FUNCS([ishexnumber isxdigit unistd.h mmap err errx warn warnx pread64 pread _lseeki64 ])

AC_TRY_COMPILE([#pragma GCC diagnostic ignored "-Wredundant-decls"],[int a=3;],
  [AC_DEFINE(HAVE_DIAGNOSTIC_REDUNDANT_DECLS,1,[define 1 if GCC supports -Wredundant-decls])]
)
AC_TRY_COMPILE([#pragma GCC diagnostic ignored "-Wcast-align"],[int a=3;],
  [AC_DEFINE(HAVE_DIAGNOSTIC_CAST_ALIGN,1,[define 1 if GCC supports -Wcast-align])]
)


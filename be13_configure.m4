#
# mix-ins for be13
#

AC_MSG_NOTICE([Including be13_configure.m4 from be13_api])

# Endian check. Used for sbuf code.
AC_C_BIGENDIAN([AC_DEFINE(BE13_API_BIGENDIAN, 1, [Big Endian aarchitecutre - like M68K])],
                AC_DEFINE(BE13_API_LITTLEENDIAN, 1, [Little Endian aarchitecutre - like x86]))


AC_CHECK_HEADERS([ dlfcn.h fcntl.h limits.h limits/limits.h linux/if_ether.h net/ethernet.h netinet/if_ether.h netinet/in.h pcap.h pcap/pcap.h sqlite3.h sys/cdefs.h sys/mman.h sys/stat.h sys/sysctl.h sys/time.h sys/types.h sys/vmmeter.h unistd.h windows.h windows.h windowsx.h winsock2.h wpcap/pcap.h mach-o/dyld.h])

AC_CHECK_FUNCS([gmtime_r ishexnumber isxdigit localtime_r unistd.h mmap err errx warn warnx pread64 pread strptime _lseeki64 utimes ])

AC_CHECK_LIB([sqlite3],[sqlite3_libversion])
AC_CHECK_FUNCS([sqlite3_create_function_v2])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#pragma GCC diagnostic ignored "-Wredundant-decls"
  int a=3;
]])],
 [AC_DEFINE(HAVE_DIAGNOSTIC_REDUNDANT_DECLS,1,[define 1 if GCC supports -Wredundant-decls])]
)

AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#pragma GCC diagnostic ignored "-Wcast-align"
 int a=3;
  ]])],
 [AC_DEFINE(HAVE_DIAGNOSTIC_CAST_ALIGN,1,[define 1 if GCC supports -Wcast-align])]
)

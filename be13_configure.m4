#
# mix-ins for be13
#

AC_MSG_NOTICE([Including be13_configure.m4 from be13_api])

AC_CHECK_HEADERS([assert.h ctype.h dirent.h dlfcn.h err.h errno.h fcntl.h inttypes.h linux/if_ether.h net/ethernet.h netinet/if_ether.h netinet/in.h pcap.h pcap/pcap.h pthread.h regex.h sbuf.h sqlite3.h stdarg.h stdint.h stdio.h stdlib.h string.h sys/cdefs.h sys/mman.h sys/stat.h sys/time.h sys/types.h tre/tre.h unistd.h windows.h windows.h windowsx.h winsock2.h wpcap/pcap.h])

AC_LANG_PUSH(C++)
    AC_CHECK_HEADERS([algorithm cassert cstdarg exception fstream iomanip iostream map regex set sstream string unordered_map unordered_set vector])
AC_LANG_POP()    


AC_CHECK_FUNCS([gmtime_r ishexnumber isxdigit localtime_r unistd.h mmap err errx warn warnx pread64 pread strptime _lseeki64 utimes ])

AC_CHECK_LIB([sqlite3],[sqlite3_libversion])
AC_CHECK_FUNCS([sqlite3_create_function_v2])

AC_TRY_COMPILE([#pragma GCC diagnostic ignored "-Wredundant-decls"],[int a=3;],
  [AC_DEFINE(HAVE_DIAGNOSTIC_REDUNDANT_DECLS,1,[define 1 if GCC supports -Wredundant-decls])]
)
AC_TRY_COMPILE([#pragma GCC diagnostic ignored "-Wcast-align"],[int a=3;],
  [AC_DEFINE(HAVE_DIAGNOSTIC_CAST_ALIGN,1,[define 1 if GCC supports -Wcast-align])]
)

AC_TRY_LINK([#include <inttypes.h>],
               [uint64_t ul; __sync_add_and_fetch(&ul,0);],
               AC_DEFINE(HAVE___SYNC_ADD_AND_FETCH,1,[define 1 if __sync_add_and_fetch works on 64-bit numbers]))



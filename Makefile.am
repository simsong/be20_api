# be20_api Makefile.am
# This file is compiled with automake to create Makefile.in.
# Makefile.in is transformed by "configure" to create Makefile
#
# (C) 2020-2022 Simson L. Garfinkel
# (C) 2020-2023 BasisTech LLC
# https://www.gnu.org/licenses/lgpl-3.0.en.html

# don't include bootstrap. People run it, and they shouldn't
# It's only for people who check out the git repo

# Use the current directory and include the list of BE20_API sources
BE20_API_DIR = .
include Makefile.defs

DFXML_SRC_DIR=dfxml_cpp/src/
include $(DFXML_SRC_DIR)Makefile.defs

# Hardcode dfxml_cpp/src/Makefile.defs ; there is a typo somewhere.
EXTRA_DIST = \
	$(DFXML_EXTRA_DIST) \
	$(BE20_API_EXTRA_DIST) \
	bootstrap.sh \
	test_be20_api_malloc_debug \
	tests/random.dat \
	tests/regex_demo.cpp \
	tests/unilang.htm \
	tests/unilang8.htm

ETAGS = etags-emacs
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = @RE2_CFLAGS@ -I$(top_srcdir)/utfcpp/source


clean-local:
	rm -f *.gcov *~ *.gcda *.gcno

clean-gcov:
	rm -f *.gcov *.gcda *.gcno

clang-format:
	clang-format* -i *h *cpp

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = test_be20_api
check_PROGRAMS = test_be20_api
check_SCRIPTS = test_be20_api_malloc_debug
TESTS = $(check_PROGRAMS)

# apitest: test_be20_api

test_be20_api_SOURCES = $(BE20_API_SRC) $(DFXML_READER) $(DFXML_WRITER) \
	catch.hpp \
	test_be20_api.cpp \
	test_be20_threadpool.cpp \
	test_image_reader.h \
	test_image_reader.cpp

#test_be20_api_CPPFLAGS = $(AM_CPPFLAGS) @RE2_CFLAGS@
test_be20_api_LDADD = @RE2_LIBS@ $(LIBS)

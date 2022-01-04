# be13_api Makefile.am
# This file is compiled with automake to create Makefile.in.
# Makefile.in is transformed by "configure" to create Makefile
#
# (C) 2020 Simson L. Garfinkel
# https://www.gnu.org/licenses/lgpl-3.0.en.html

# don't include bootstrap. People run it, and they shouldn't
# It's only for people who check out the git repo

# Use the current directory and include the list of BE13_API sources
BE13_API_DIR = .
include Makefile.defs

DFXML_SRC_DIR=dfxml_cpp/src/
include $(DFXML_SRC_DIR)Makefile.defs

EXTRA_DIST = \
	$(DFXML_EXTRA_DIST) \
	$(BE13_API_EXTRA_DIST) \
	Makefile.defs \
	bootstrap.sh \
	m4/slg_gcc_all_warnings.m4 \
	test_be13_api_malloc_debug \
	tests/random.dat \
	tests/regex_demo.cpp \
	tests/unilang.htm \
	tests/unilang8.htm


ETAGS = etags-emacs
ACLOCAL_AMFLAGS = -I m4


clean-local:
	rm -f *.gcov *~ *.gcda *.gcno

clean-gcov:
	rm -f *.gcov *.gcda *.gcno

clang-format:
	clang-format* -i *h *cpp

AUTOMAKE_OPTIONS = subdir-objects

#noinst_LIBRARIES = libbe13api.a
#libbe13api_a_SOURCES = $(BE13_API_SRC)

bin_PROGRAMS = test_be13_api
check_PROGRAMS = test_be13_api
check_SCRIPTS = test_be13_api_malloc_debug
TESTS = $(check_PROGRAMS)

# apitest: test_be13_api

test_be13_api_SOURCES = $(BE13_API_SRC) $(DFXML_READER) $(DFXML_WRITER) \
	catch.hpp \
	test_be13_api.cpp \
	test_be13_threadpool.cpp \
	test_image_reader.h \
	test_image_reader.cpp

push:
	make && make check && make distcheck && git push origin

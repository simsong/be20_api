# be13_api Makefile.am
# This file is compiled with automake to create Makefile.in.
# Makefile.in is transformed by "configure" to create Makefile
#
# (C) 2020 Simson L. Garfinkel
# https://www.gnu.org/licenses/lgpl-3.0.en.html

# don't include bootstrap. People run it, and they shouldn't
# It's only for people who check out the git repo

EXTRA_DIST = \
	bootstrap.sh \
	m4/slg_gcc_all_warnings.m4 \
	test_be13_api_malloc_debug \
	tests/random.dat \
	tests/regex_demo.cpp \
	tests/unilang.htm \
	tests/unilang8.htm

ETAGS = etags-emacs
ACLOCAL_AMFLAGS = -I m4

# Use the current directory
BE13_API_DIR=
include Makefile.defs

clean-local:
	rm -f *.gcov *~ *.gcda *.gcno

clean-gcov:
	rm -f *.gcov *.gcda *.gcno

#DFXML_SRC_DIR=dfxml/src/
#include dfxml/src/Makefile.defs

DFXML_WRITER = dfxml/src/dfxml_writer.cpp \
      dfxml/src/dfxml_writer.h \
      dfxml/src/hash_t.h \
      dfxml/src/cpuid.h

AUTOMAKE_OPTIONS = subdir-objects

#noinst_LIBRARIES = libbe13api.a
#libbe13api_a_SOURCES = $(BE13_API_SRC)

bin_PROGRAMS = test_be13_api
check_PROGRAMS = test_be13_api
check_SCRIPTS = test_be13_api_malloc_debug
TESTS = $(check_PROGRAMS)

# apitest: test_be13_api

# Slowly add modules as tests are written for them.
#	feature_recorder_mhist.cpp
#	feature_recorder_mhist.h

test_be13_api_SOURCES = $(DFXML_WRITER) test_be13_api.cpp catch.hpp \
	aftimer.h \
	atomic_map.h \
	atomic_set.h \
	atomic_unicode_histogram.cpp \
	atomic_unicode_histogram.h \
	bulk_extractor_i.h \
	char_class.h \
	feature_recorder.cpp \
	feature_recorder.h 	\
	feature_recorder_file.cpp \
	feature_recorder_file.h \
	feature_recorder_set.cpp \
	feature_recorder_set.h 	\
	feature_recorder_sql.cpp \
	feature_recorder_sql.h \
	histogram_def.cpp \
	histogram_def.h  \
	net_ethernet.h \
	packet_info.h \
	pcap_fake.cpp \
	pcap_fake.h \
	pos0.h \
	regex_vector.cpp \
	regex_vector.h \
	sbuf.cpp \
	sbuf.h \
	sbuf_private.h \
	scan_sha1_test.cpp \
	scan_sha1_test.h \
	scanner_config.cpp \
	scanner_config.h \
	scanner_params.cpp \
	scanner_params.h \
	scanner_set.cpp \
	scanner_set.h \
	unicode_escape.cpp \
	unicode_escape.h \
	utf8.h \
	utf8/checked.h \
	utf8/core.h \
	utf8/cpp11.h \
	utf8/cpp17.h \
	utf8/LICENSE \
	utf8/README.md \
	utf8/unchecked.h \
	utils.cpp \
	utils.h \
	word_and_context_list.cpp \
	word_and_context_list.h

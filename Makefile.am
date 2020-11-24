# be13_api Makefile.am
# This file is compiled with automake to create Makefile.in.
# Makefile.in is transformed by "configure" to create Makefile
#
# (C) 2020 Simson L. Garfinkel
# https://www.gnu.org/licenses/lgpl-3.0.en.html

# don't include bootstrap. People run it, and they shouldn't
# It's only for people who check out the git repo

EXTRA_DIST = \
	cester_configure.m4 \
	slg_gcc_all_warnings.m4

ETAGS = etags-emacs
ACLOCAL_AMFLAGS = -I m4

# Use the current directory
BE13_API_DIR=
include Makefile.defs

clean-local:
	rm -f *.gcov *~ *.gcda *.gcno

clean-gcov:
	rm -f *.gcov *.gcda *.gcno

DFXML_SRC_DIR=dfxml/src/
#include dfxml/src/Makefile.defs

DFXML_WRITER = dfxml/src/dfxml_writer.cpp \
      dfxml/src/dfxml_writer.h \
      dfxml/src/hash_t.h \
      dfxml/src/cpuid.h


AUTOMAKE_OPTIONS = subdir-objects

#noinst_LIBRARIES = libbe13api.a
#libbe13api_a_SOURCES = $(BE13_API_SRC)

check_PROGRAMS = test_be13_api
TESTS = $(check_PROGRAMS)

apitest: test_be13_api

# Slowly add modules as tests are written for them.
test_be13_api_SOURCES = $(DFXML_WRITER) test_be13_api.cpp catch.hpp \
	regex_vector.h regex_vector.cpp \
	unicode_escape.h unicode_escape.cpp \
	scanner_config.h scanner_config.cpp \
	scanner_set.h scanner_set.cpp \
	scanner_params.h scanner_params.cpp \
	packet_info.h pcap_fake.h \
	histogram_def.h  \
	histogram_maker.h histogram_maker.cpp \
	feature_recorder_sql.h feature_recorder_sql.cpp \
	feature_recorder_file.h feature_recorder_file.cpp \
	feature_recorder_set.h 	feature_recorder_set.cpp \
	feature_recorder.h 	feature_recorder.cpp \
	word_and_context_list.h  word_and_context_list.cpp \
	scan_sha1.h scan_sha1.cpp \
	sbuf.h sbuf.cpp \
	utils.h

# be13_api Makefile.am
# This file is compiled with automake to create Makefile.in. 
# Makefile.in is transformed by "configure" to create Makefile
#
# (C) 2020 Simson L. Garfinkel
# https://www.gnu.org/licenses/lgpl-3.0.en.html

# don't include bootstrap. People run it, and they shoudln't
# It's only for people who check out the git repo

EXTRA_DIST = \
	cester_configure.m4 \
	slg_gcc_all_warnings.m4

ACLOCAL_AMFLAGS = -I m4

# Use the current directory
BE13_API_DIR=
include Makefile.defs

#DFXML_SRC_DIR=dfxml/src/
#include dfxml/src/Makefile.defs

DFXML_WRITER = dfxml/src/dfxml_writer.cpp \
      dfxml/src/dfxml_writer.h \
      dfxml/src/hash_t.h \
      dfxml/src/cpuid.h 


AUTOMAKE_OPTIONS = subdir-objects

#noinst_LIBRARIES = libbe13api.a
#libbe13api_a_SOURCES = $(BE13_API_SRC)

check_PROGRAMS = test_be13api_catch2
TESTS = $(check_PROGRAMS)

# Slowly add modules as tests are written for them.
test_be13api_catch2_SOURCES = $(BE13_API_SRC) $(DFXML_WRITER) test_be13api_catch2.cpp tests/catch2.h 

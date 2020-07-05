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

#noinst_LIBRARIES = libbe13api.a
#libbe13api_a_SOURCES = $(BE13_API_SRC)

check_PROGRAMS = test_be13api
TESTS = $(check_PROGRAMS)

test_be13api_SOURCES = test_be13api.cpp tests/cester.h regex_vector.cpp regex_vector.h

# add each test as it is working!
# Right now we have no working tests
#test_be13api_SOURCES = test_be13api.cpp tests/cester.h 

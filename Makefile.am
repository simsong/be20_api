# (C) 2020 Simson L. Garfinkel
# https://www.gnu.org/licenses/lgpl-3.0.en.html

BE13_API_DIR=""
include Makefile.defs

noinst_LIBRARIES = libbe13api.a
libbe13api_a_SOURCES = $(BE13_API_SRC)

check_PROGRAMS = test_be13api
TESTS = $(check_PROGRAMS)

test_be13api_SOURCES = test_be13api.cpp tests/cester.h $(BE13_API_SRC)

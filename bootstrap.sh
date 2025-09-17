#!/bin/sh

mkdir -p build-aux

# have automake do an initial population if necessary
autoheader -f
touch NEWS README AUTHORS ChangeLog
touch stamp-h
aclocal -I m4
autoconf -f
automake --add-missing --copy
# bootstrap is complete
echo
echo The bootstrap.sh is complete.  Be sure to run ./configure.
echo

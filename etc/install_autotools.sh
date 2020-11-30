#!/bin/sh

# Originally from https://gist.github.com/GraemeConradie/49d2f5962fa72952bc6c64ac093db2d5
# Install gnu autotools for running under github actions

##
# Install autoconf, automake and libtool smoothly on Mac OS X.
# Newer versions of these libraries are available and may work better on OS X
##

export build=~/devtools             # or wherever you'd like to build
export PATH=$PATH:/usr/local/bin
mkdir -p $build

##
# Autoconf
# https://ftpmirror.gnu.org/autoconf

AUTOCONF="autoconf-2.69"
cd $build
curl -k -OL https://ftpmirror.gnu.org/autoconf/$AUTOCONF.tar.gz || exit 1
ls -l $AUTOCONF.tar.gz
tar xzf $AUTOCONF.tar.gz  || exit 1
cd $AUTOCONF
./configure --prefix=/usr/local || exit 1
make || exit 1
sudo make install || exit 1

##
# Automake
# https://ftpmirror.gnu.org/automake

AUTOMAKE="automake-1.16.3"
cd $build
curl -k -OL https://ftpmirror.gnu.org/automake/$AUTOMAKE.tar.gz || exit 1
ls -l $AUTOMAKE.tar.gz
tar xzf $AUTOMAKE.tar.gz
cd $AUTOMAKE
./configure --prefix=/usr/local
make
sudo make install

##
# Libtool
# https://ftpmirror.gnu.org/libtool

LIBTOOL=libtool-2.4.6
cd $build
curl -k -OL https://ftpmirror.gnu.org/libtool/$LIBTOOL.tar.gz || exit 1
ls -l $LIBTOOL.tar.gz
tar xzf $LIBTOOL.tar.gz
cd $LIBTOOL
./configure --prefix=/usr/local
make
sudo make install

echo "Installation complete."

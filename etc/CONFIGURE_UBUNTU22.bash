#!/bin/bash
# cd to the directory where the script is
# http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in
OS_NAME=ubuntu
OS_VERSION=22
MAKE_CONCURRENCY=-j2
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $SCRIPT_DIR

. ./paths.bash

if [ ! -r /etc/os-release ]; then
  echo This requires /etc/os-release
  exit 1
fi
. /etc/os-release
if [ $ID != $OS_NAME ]; then
    echo This requires $OS_NAME Linux. You have $ID.
    exit 1
fi

if [ $VERSION_ID -ne $OS_VERSION ]; then
    echo This requires $OS_NAME version $OS_VERSION. You have $ID $VERSION_ID.
    exit 1
fi
cat <<EOF
******************************************************************
Configuring $OS_NAME $OS_VERSION to compile be20 for test programs
******************************************************************
EOF

MPKGS="autoconf automake g++ libtool libssl-dev pkg-config libabsl-dev libre2-dev libsqlite3-dev make "

sudo apt update -y
sudo apt install -y $MPKGS
if [ $? != 0 ]; then
  echo "Could not install some of the packages. Will not proceed."
  exit 1
fi

# Do these need to be built from sources?
# wget --no-check-certificate https://ftpmirror.gnu.org/autoconf/autoconf-2.71.tar.gz
# tar xfz autoconf-2.71.tar.gz && cd autoconf-2.71 && ./configure && make && sudo make install

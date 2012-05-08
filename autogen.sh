#!/bin/sh

[ -d support ] || mkdir support || exit

echo -n "Running autoreconf..."
autoreconf -i -s || exit
echo " done"

echo "Running configure $*..."

export CFLAGS="`dpkg-buildflags --get CFLAGS` -Wall -pedantic"
export LDFLAGS="`dpkg-buildflags --get LDFLAGS`"
export CPPFLAGS="`dpkg-buildflags --get CPPFLAGS`"
./configure $*

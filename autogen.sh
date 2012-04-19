#!/bin/sh

[ -d support ] || mkdir support || exit

echo -n "Running autoreconf..."
autoreconf -i || exit
echo " done"

echo "Running configure $*..."
CFLAGS="-Wall -O2 -g" ./configure $*

#!/bin/sh

[ -d support ] || mkdir support || exit

echo -n "Running autoreconf..."
autoreconf -i -s || exit
echo " done"

echo -n "Running intltoolize..."
intltoolize -f --automake || exit
echo " done"

echo "Running configure $*..."
CFLAGS="-Wall -O2 -g" ./configure $*

#!/bin/sh

[ -d support ] || mkdir support || exit

echo -n "Running glib-gettextize..."
glib-gettextize -f -c || exit
echo " done"

echo -n "Running autoreconf..."
autoreconf -f -i || exit
echo " done"

echo "Running configure $*..."
CFLAGS="-Wall -O2 -g $CFLAGS" ./configure $*

#!/bin/sh

echo -n "Running aclocal..."
aclocal -I cf || exit
echo " done"

echo -n "Running autoheader..."
autoheader || exit
echo " done"

echo -n "Running autoconf..."
autoconf || exit
echo " done"

echo -n "Running automake..."
automake || exit
echo " done"

echo "Running configure $*..."
CFLAGS="-Wall -O2 -g" ./configure $*

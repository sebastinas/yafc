#!/bin/bash

# Script to compile yafc into yafc.exe using mingw32
# Run this script from the base directory as so:
#    contrib/mingw32.sh
#
# This script has the following requirements:
#  - mingw32
#
# On Debian/Ubuntu:
#    apt-get install mingw32
#

make distclean

mkdir -p build_mingw32

cp lib/fnmatch.h fnmatch_.h

./configure \
	--build=i586-mingw32msvc \
	--host=i586-mingw32msvc \
	--without-socks \
	--without-socks5 \
	--without-readline \
	--without-krb4 \
	--without-krb5 \
	--without-gssapi \
	--without-bash-completion

make

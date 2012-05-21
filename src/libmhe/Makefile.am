## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libmhe.a

libmhe_a_SOURCES = linklist.c strq.c shortpath.c args.c

noinst_HEADERS = linklist.h strq.h shortpath.h args.h

AM_CPPFLAGS = -I$(srcdir) \
							-I$(top_builddir) \
							-I$(top_srcdir)/lib \
							-I$(top_srcdir)/src \
							-I$(top_srcdir)/src/libmhe

DEFS=@DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

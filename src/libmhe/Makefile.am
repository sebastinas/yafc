## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2001/05/12 18:43:01 mhe Exp $

noinst_LIBRARIES = libmhe.a

libmhe_a_SOURCES = linklist.c strq.c shortpath.c args.c

noinst_HEADERS = linklist.h strq.h shortpath.h args.h

INCLUDES = -I$(srcdir) -I$(top_builddir) -I$(top_srcdir)/lib \
-I$(top_builddir)/intl -I$(top_srcdir)/src -I$(top_srcdir)/src/libmhe

localedir = $(datadir)/locale

DEFS=-DLOCALEDIR=\"$(localedir)\" @DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

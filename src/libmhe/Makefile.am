## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2001/05/13 11:57:45 mhe Exp $

noinst_LIBRARIES = libmhe.a

libmhe_a_SOURCES = linklist.c strq.c shortpath.c args.c

noinst_HEADERS = linklist.h strq.h shortpath.h args.h

INCLUDES = -I$(srcdir) -I$(top_builddir) -I$(top_srcdir)/lib \
           -I$(top_srcdir)/src -I$(top_srcdir)/src/libmhe

DEFS=@DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

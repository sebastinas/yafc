## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libcfg.a

libcfg_a_SOURCES = cfglib.c cfgread.c cfgsave.c cfgutil.c cfgfree.c

noinst_HEADERS = cfglib.h

INCLUDES = -I$(srcdir) -I$(top_builddir) -I$(top_srcdir)/lib \
-I$(top_builddir)/intl -I$(top_srcdir)/src -I$(top_srcdir)/src/libmhe

localedir = $(datadir)/locale

DEFS=-DLOCALEDIR=\"$(localedir)\" @DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

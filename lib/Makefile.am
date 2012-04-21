## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libfoo.a

libfoo_a_SOURCES = xmalloc.c \
									 makepath.c \
									 base64.c \
									 modechange.c \
                   bashline.c

noinst_HEADERS = xmalloc.h \
								 getopt.h \
								 strcasecmp.h \
								 krb4.h \
								 base64.h \
								 pathmax.h \
								 makepath.h \
								 modechange.h \
								 security.h \
								 collsyms.h \
								 bashline.h \
								 fnmatch_.h

libfoo_a_LIBADD = @LIBOBJS@ @ALLOCA@
libfoo_a_DEPENDENCIES = $(libfoo_a_LIBADD)

INCLUDES = -I.. -I$(srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/src/ftp \
-I$(top_srcdir)/src/libmhe

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

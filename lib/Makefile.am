## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libfoo.a

libfoo_a_SOURCES = xmalloc.c fnmatch.c makepath.c base64.c glob.c modechange.c

noinst_HEADERS = xmalloc.h fnmatch.h getopt.h \
                 glob.h strcasecmp.h krb4.h base64.h \
				 setproctitle.h pathmax.h makepath.h modechange.h \
				 security.h collsyms.h

libfoo_a_LIBADD = @LIBOBJS@ @ALLOCA@
libfoo_a_DEPENDENCIES = $(libfoo_a_LIBADD)

INCLUDES = -I.. -I$(srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/src/ftp \
-I$(top_srcdir)/src/libmhe -I../intl

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libftp.a

libftp_a_SOURCES = ftp.c socket.c host.c rglob.c rfile.c \
                   rdirectory.c lscolors.c url.c cache.c ftpsend.c \
				   ftpsigs.c atomicio.c buffer.c bufaux.c \
				   sftp-common.c ssh_cmd.c ssh_ftp.c

noinst_HEADERS = ftp.h socket.h host.h rglob.h rfile.h rdirectory.h url.h \
                 ftpsigs.h ssh.h atomicio.h buffer.h bufaux.h \
				 sftp-common.h pathnames.h getput.h \
				 ssh_cmd.h ssh_ftp.h

INCLUDES = -I$(srcdir) -I$(top_builddir) -I$(top_srcdir)/lib \
-I$(top_builddir)/intl -I$(top_srcdir)/src -I$(top_srcdir)/src/libmhe

localedir = $(datadir)/locale

DEFS=-DLOCALEDIR=\"$(localedir)\" @DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

celan: clean

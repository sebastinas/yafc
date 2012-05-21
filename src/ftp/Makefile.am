## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libftp.a

if HAVE_LIBSSH
SSHSRCS = ssh_cmd.c
else
SSHSRCS =
endif

libftp_a_SOURCES = ftp.c \
									 socket.c \
									 host.c \
									 rglob.c \
									 rfile.c \
                   rdirectory.c \
									 lscolors.c \
									 url.c \
									 cache.c \
									 ftpsend.c \
									 ftpsigs.c \
									 $(SSHSRCS)

noinst_HEADERS = ftp.h \
								 socket.h \
								 host.h \
								 rglob.h \
								 rfile.h \
								 rdirectory.h \
								 url.h \
								 ftpsigs.h \
								 ssh_cmd.h \
								 lscolors.h

AM_CPPFLAGS = $(SSH_CFLAGS) \
							-I$(srcdir) \
							-I$(top_builddir) \
							-I$(top_srcdir)/lib \
							-I$(top_srcdir)/src \
							-I$(top_srcdir)/src/libmhe

DEFS=@DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/*

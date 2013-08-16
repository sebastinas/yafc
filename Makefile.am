## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-xz no-dist-gzip
EXTRA_DIST = BUGS THANKS COPYRIGHT README.md \
						 samples/yafcrc \
						 samples/inputrc \
						 contrib/yafc-import_ncftp.pl \
						 completion/yafc \
						 src/ftp/ssh_cmd.c \
						 lib/base64.c
if USE_NLS
SUBDIRS = doc po
else
SUBDIRS = doc
endif

ACLOCAL_AMFLAGS = -I m4

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build yafcrc.h .deps/*

if USE_BASH_COMPLETION
bash_completion_DATA = completion/yafc
else
bash_completion_DATA =
endif

if HAVE_LIBSSH
SSHSRCS = src/ftp/ssh_cmd.c
else
SSHSRCS =
endif

bin_PROGRAMS = yafc

yafc_SOURCES = src/main.c \
							 src/alias.c \
							 src/cmd.c \
							 src/commands.c \
							 src/completion.c \
							 src/get.c \
							 src/fxp.c \
							 src/gvars.c \
							 src/lglob.c \
							 src/help.c \
							 src/input.c \
							 src/local.c \
							 src/login.c \
							 src/list.c \
							 src/put.c \
							 src/rm.c \
							 src/tag.c \
							 src/redir.c \
							 src/prompt.c \
							 src/transfer.c \
							 src/rc.c \
							 src/set.c \
							 src/stats.c \
							 src/ltag.c \
							 src/bookmark.c \
							 src/utils.c \
							 src/libmhe/linklist.c \
							 src/libmhe/strq.c \
							 src/libmhe/shortpath.c \
							 src/libmhe/args.c \
							 src/libmhe/xmalloc.c \
							 src/ftp/ftp.c \
							 src/ftp/socket.c \
							 src/ftp/plain-socket.c \
							 src/ftp/host.c \
							 src/ftp/rglob.c \
							 src/ftp/rfile.c \
               src/ftp/rdirectory.c \
							 src/ftp/lscolors.c \
							 src/ftp/url.c \
							 src/ftp/cache.c \
							 src/ftp/ftpsend.c \
							 src/ftp/ftpsigs.c \
							 src/utils/modechange.c \
							 $(SSHSRCS) \
							 lib/makepath.c \
							 lib/base64.c \
							 src/utils//bashline.c

noinst_HEADERS = src/alias.h \
								 src/cmd.h \
								 src/completion.h \
								 src/commands.h \
								 src/transfer.h \
								 src/gvars.h \
								 src/help.h \
								 src/input.h \
								 src/get.h \
								 src/put.h \
								 src/lglob.h \
								 src/set.h \
								 src/stats.h \
								 src/syshdr.h \
								 src/redir.h \
								 src/login.h \
								 src/utils.h \
								 src/bookmark.h \
								 src/tag.h \
								 src/ltag.h \
								 src/prompt.h \
								 src/redir.h \
								 src/rc.h \
								 src/ftp/ftp.h \
								 src/ftp/socket.h \
								 src/ftp/socket-impl.h \
								 src/ftp/host.h \
								 src/ftp/rglob.h \
								 src/ftp/rfile.h \
								 src/ftp/rdirectory.h \
								 src/ftp/url.h \
								 src/ftp/ftpsigs.h \
								 src/ftp/ssh_cmd.h \
								 src/ftp/lscolors.h \
								 src/libmhe/linklist.h \
								 src/libmhe/strq.h \
								 src/libmhe/shortpath.h \
								 src/libmhe/args.h \
								 src/libmhe/xmalloc.h \
								 src/utils/modechange.h \
								 lib/getopt.h \
								 lib/base64.h \
								 lib/makepath.h \
								 lib/security.h \
								 lib/collsyms.h \
								 src/utils/bashline.h \
								 lib/fnmatch_.h

yafc_LDADD = $(SSH_LIBS) \
						 @LIBOBJS@ \
						 $(INTLLIBS) \
						 $(BSD_LIBS) \
						 $(EDITLINE_LIBS)

DEFS = -DLOCALEDIR=\"${YAFC_LOCALEDIR}\" \
			 -DSYSCONFDIR=\"@sysconfdir@\" \
			 @DEFS@

BUILT_SOURCES = yafcrc.h

AM_CPPFLAGS = $(SSH_CFLAGS) \
							$(EDITLINE_CFLAGS) \
							-I$(top_srcdir)/src \
							-I$(top_srcdir)/lib \
							-I$(top_srcdir)/src/ftp \
							-I$(top_srcdir)/src/libmhe

yafcrc.h: $(top_srcdir)/samples/yafcrc
	echo "const char *default_yafcrc = \"\"" > yafcrc.h
	cat $(top_srcdir)/samples/yafcrc | sed 's/\\/\\\\/g' | sed 's/"/\\\"/g' | sed 's/^\(.*\)$$/\"\1\\n\"/' >> yafcrc.h
	echo ";" >> yafcrc.h

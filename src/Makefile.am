## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2001/05/13 11:55:36 mhe Exp $

SUBDIRS = libmhe ftp

bin_PROGRAMS = yafc

noinst_PROGRAMS = text2c

text2c_SOURCES = text2c.c

BUILT_SOURCES = yafcrc.h

yafc_SOURCES = main.c \
   alias.c cmd.c commands.c completion.c get.c fxp.c gvars.c lglob.c \
   help.c input.c local.c login.c list.c put.c rm.c tag.c \
   redir.c prompt.c transfer.c rc.c set.c ltag.c bookmark.c utils.c

noinst_HEADERS = \
   alias.h cmd.h completion.h commands.h transfer.h \
   gvars.h help.h input.h get.h put.h lglob.h \
   set.h syshdr.h redir.h login.h utils.h bookmark.h tag.h ltag.h prompt.h \
   redir.h rc.h

yafc_LDADD = ftp/libftp.a libmhe/libmhe.a ../lib/libfoo.a

yafc_DEPENDENCIES = $(yafc_LDADD) yafcrc.h
INCLUDES = -I. -I.. -I$(srcdir) -I$(top_srcdir)/lib \
-I$(top_srcdir)/src/ftp -I$(top_srcdir)/src/libmhe

DEFS=@DEFS@

CLEANFILES=*~ \#*\#
DISTCLEANFILES=.deps/* yafcrc.h

yafcrc.h: text2c $(top_srcdir)/yafcrc.sample
	./text2c $(top_srcdir)/yafcrc.sample yafcrc.h default_yafcrc

## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.5 2001/11/03 12:07:55 mhe Exp $

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT acconfig.h \
             yafcrc.sample inputrc.sample \
             yafc.spec yafc.spec.in

SUBDIRS = doc lib src

CONTRIB_FILES = yafc-import_ncftp.pl

dist-hook:
	mkdir $(distdir)/contrib
	for cf in $(CONTRIB_FILES); do \
		cp -p $(top_srcdir)/contrib/$$cf $(distdir)/contrib ; done

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

celan: clean

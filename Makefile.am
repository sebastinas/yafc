## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2001/05/13 12:00:47 mhe Exp $

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT acconfig.h \
             yafcrc.sample inputrc.sample \
             yafc.spec yafc.spec.in

SUBDIRS = doc lib src

SUPPORT_FILES = config.guess config.sub install-sh mdate-sh \
                missing mkinstalldirs
CONTRIB_FILES = yafc-import_ncftp.pl

dist-hook:
	mkdir $(distdir)/support
	for sf in $(SUPPORT_FILES) ; do \
		cp -p $(top_srcdir)/support/$$sf $(distdir)/support ; done
	mkdir $(distdir)/contrib
	for cf in $(CONTRIB_FILES); do \
		cp -p $(top_srcdir)/contrib/$$cf $(distdir)/contrib ; done

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

celan: clean

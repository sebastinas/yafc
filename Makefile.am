## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.6 2002/02/23 13:16:30 mhe Exp $

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT acconfig.h \
             yafcrc.sample inputrc.sample

SUBDIRS = doc lib src

CONTRIB_FILES = yafc-import_ncftp.pl yafc-mandrake.spec \
 yafc-mandrake.spec.in yafc-rw.spec yafc-rw.spec.in

SUPPORT_FILES = config.guess config.sub depcomp install-sh ltconfig \
 ltmain.sh mdate-sh missing mkinstalldirs texinfo.tex

dist-hook:
	mkdir $(distdir)/contrib
	for cf in $(CONTRIB_FILES); do \
		cp -p $(top_srcdir)/contrib/$$cf $(distdir)/contrib ; done
	test -d $(distdir)/support || mkdir $(distdir)/support; \
	for sf in $(SUPPORT_FILES) ; do \
		cp -p $(top_srcdir)/support/$$sf $(distdir)/support ; done

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

celan: clean

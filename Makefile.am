## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.7 2002/05/28 09:47:51 mhe Exp $

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT acconfig.h \
             yafcrc.sample inputrc.sample

SUBDIRS = doc lib src

CONTRIB_FILES = yafc-import_ncftp.pl yafc-mandrake.spec \
 yafc-mandrake.spec.in yafc-rw.spec yafc-rw.spec.in

SUPPORT_FILES = config.guess config.sub depcomp install-sh ltconfig \
 ltmain.sh mdate-sh missing mkinstalldirs texinfo.tex

dist-hook:
	test -d $(distdir)/contrib || mkdir $(distdir)/contrib; \
	for cf in $(CONTRIB_FILES) ; do \
		cp -p $(top_srcdir)/contrib/$$cf $(distdir)/contrib ; done
	test -d $(distdir)/support || mkdir $(distdir)/support; \
	for sf in $(SUPPORT_FILES) ; do \
		cp -p $(top_srcdir)/support/$$sf $(distdir)/support ; done

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

celan: clean

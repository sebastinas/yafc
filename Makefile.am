## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.9 2003/07/12 10:25:41 mhe Exp $

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT \
             yafcrc.sample inputrc.sample

SUBDIRS = doc lib src

ACLOCAL_AMFLAGS = -I cf

CONTRIB_FILES = yafc-import_ncftp.pl yafc-mandrake.spec \
 yafc-mandrake.spec.in yafc-rw.spec yafc-rw.spec.in

dist-hook:
	test -d $(distdir)/contrib || mkdir $(distdir)/contrib; \
	for cf in $(CONTRIB_FILES) ; do \
		cp -p $(top_srcdir)/contrib/$$cf $(distdir)/contrib ; done

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

celan: clean

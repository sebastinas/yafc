## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = ABOUT-NLS BUGS THANKS COPYRIGHT acconfig.h \
             yafcrc.sample inputrc.sample \
             yafc.spec yafc.spec.in

SUBDIRS = intl po doc lib src

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

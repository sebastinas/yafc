## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = ABOUT-NLS BUGS THANKS COPYRIGHT acconfig.h \
             yafcrc.sample inputrc.sample \
             yafc.spec yafc.spec.in

SUBDIRS = intl po doc lib src

dist-hook:
	mkdir $(distdir)/support
	cp -p $(top_srcdir)/support/* $(distdir)/support
	mkdir $(distdir)/contrib
	cp -p $(top_srcdir)/contrib/* $(distdir)/contrib

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.8 2002/12/05 22:15:08 mhe Exp $

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT \
             yafcrc.sample inputrc.sample

SUBDIRS = doc lib src

CONTRIB_FILES = yafc-import_ncftp.pl yafc-mandrake.spec \
 yafc-mandrake.spec.in yafc-rw.spec yafc-rw.spec.in

SUPPORT_FILES = config.guess config.sub depcomp install-sh ltconfig \
 ltmain.sh mdate-sh missing mkinstalldirs texinfo.tex

CF_FILES = arg-with.m4 bash-func-posix-setjmp.m4 bash-signal-check.m4 \
 check-types.m4 kerberos.m4 readline.m4 ssl.m4

dist-hook:
	test -d $(distdir)/contrib || mkdir $(distdir)/contrib; \
	for cf in $(CONTRIB_FILES) ; do \
		cp -p $(top_srcdir)/contrib/$$cf $(distdir)/contrib ; done
	test -d $(distdir)/support || mkdir $(distdir)/support; \
	for sf in $(SUPPORT_FILES) ; do \
		cp -p $(top_srcdir)/support/$$sf $(distdir)/support ; done
	test -d $(distdir)/cf || mkdir $(distdir)/cf; \
	for f in $(CF_FILES) ; do \
		cp -p $(top_srcdir)/cf/$$f $(distdir)/cf ; done

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

celan: clean

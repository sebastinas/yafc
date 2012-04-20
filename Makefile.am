## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = BUGS THANKS COPYRIGHT \
						 yafcrc.sample \
						 inputrc.sample \
						 contrib/yafc-import_ncftp.pl \
						 completion/yafc

SUBDIRS = doc lib src

ACLOCAL_AMFLAGS = -I cf

CLEANFILES=*~ \#*\#
DISTCLEANFILES=build

if USE_BASH_COMPLETION
bash_completion_DATA = completion/yafc
else
bash_completion_DATA =
endif

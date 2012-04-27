/*
 * commands.c --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "commands.h"
#include "cmd.h"
#include "alias.h"
#include "ftp.h"
#include "strq.h"
#include "gvars.h"
#include "bashline.h"
#include "utils.h"
#include "bookmark.h"

#undef __  /* gettext no-op */
#define __(text) (text)

#undef CMD
#define CMD(NAME, NC, NL, AU, CPL) \
 {#NAME, NC, NL, AU, CPL, cmd_ ## NAME}

cmd_t cmds[] = {
	{"!", 0,0,0, cpLocalFile, cmd_shell},
	CMD(alias, 0,0,1, cpAlias),
	CMD(bookmark, 0,0,1, cpBookmark),
	CMD(cache, 0,0,1, cpNone),
	CMD(cat, 0,0,0, cpRemoteFile),
	CMD(cd, 0,0,1, cpRemoteDir),
	CMD(cdup, 0,0,1, cpNone),
	CMD(chmod, 0,0,0, cpRemoteFile),
	CMD(close, 0,0,1, cpNone),
	CMD(copyright, 0,0,1, cpNone),
	CMD(filetime, 0,0,1, cpRemoteFile),
	CMD(fxp, 0,0,0, cpRemoteFile),
	CMD(get, 0,0,0, cpRemoteFile),
	CMD(help, 0,0,1, cpCommand),
	CMD(lcd, 0,0,1, cpLocalFile),
	CMD(list, 0,0,1, cpRemoteFile),
	CMD(lpwd, 0,0,1, cpNone),
	CMD(ls, 0,0,0, cpRemoteFile),
	CMD(ltag, 0,0,0, cpLocalFile),
	CMD(luntag, 0,0,0, cpLocalTaglist),
	CMD(mkdir, 0,0,1, cpNone),
	CMD(nlist, 0,0,1, cpRemoteFile),
	CMD(nop, 0,0,1, cpNone),
	CMD(idle, 0,0,1, cpNone),
	CMD(open, 0,0,1, cpHostname),
	CMD(put, 0,0,0, cpLocalFile),
	CMD(pwd, 0,0,1, cpNone),
	CMD(quit, 0,0,1, cpNone),
	CMD(exit, 0,0,1, cpNone),
	CMD(quote, 0,0,1, cpNone),
	CMD(mv, 0,0,1, cpRemoteFile),
	CMD(reopen, 1,1,1, cpNone),
	CMD(rhelp, 1,0,1, cpNone),
	CMD(rm, 0,0,0, cpRemoteFile),
	CMD(rmdir, 0,0,1, cpRemoteDir),
	CMD(rstatus, 0,0,1, cpRemoteFile),
	CMD(set, 0,0,1, cpVariable),
	CMD(shell, 0,0,0, cpLocalFile),
	CMD(site, 0,0,1, cpRemoteFile),
	CMD(source, 0,0,1, cpLocalFile),
	CMD(status, 0,0,1, cpNone),
	CMD(switch, 0,0,0, cpFtpList),
	CMD(system, 0,0,0, cpNone),
	CMD(tag, 0,0,0, cpRemoteFile),
	CMD(unalias, 0,0,1, cpAlias),
	CMD(untag, 0,0,0, cpTaglist),
	CMD(url, 0,0,1, cpNone),
	CMD(user, 0,0,1, cpNone),
	CMD(version, 0,0,1, cpNone),
	CMD(warranty, 0,0,1, cpNone),
#ifdef HAVE_KERBEROS
	CMD(prot, 1,1,1, cpNone),
#endif
#ifdef HAVE_KRB4
	CMD(afslog, 0,0,1, cpNone),
	CMD(klist, 0,0,1, cpNone),
	CMD(kauth, 0,0,1, cpNone),
	CMD(kdestroy, 0,0,1, cpNone),
	CMD(krbtkfile, 0,0,1, cpNone),
#endif

    /* this _must_ be the last entry, unless you like segfaults ;) */
    {0, 0,0,1, 0, 0}
};

cmd_t *find_cmd(const char *cmd)
{
	int i;
	cmd_t *r = 0;

	for(i=0;cmds[i].cmd;i++) {
		/* compare only strlen(cmd) chars, allowing commands
		 * to be shortened, as long as they're not ambiguous
		 */
		if(strncmp(cmds[i].cmd, cmd, strlen(cmd)) == 0) {
			/* is this an exact match? */
			if(strlen(cmd) == strlen(cmds[i].cmd))
				return &cmds[i];
			if(r)
				r = CMD_AMBIGUOUS;
			else
				r = &cmds[i];
		}
	}
	return r;
}

int rearrange_args(args_t *args, const char *rchars)
{
	int r = -1;
	int i;

	for(i=0; i<args->argc; i++) {
		char *e = args->argv[i];
		while(*e) {
			if((strchr(rchars, *e) != 0) &&
			   (!char_is_quoted(args->argv[i], (int)(e - args->argv[i]))))
				{
					r = i;
					break;
				}
			e++;
		}
		if(*e && e != args->argv[i]) {
			char *a;
			size_t num;
#if 0
			if(e[-1] == '\\')
				/* skip backslash-quoted chars */
				continue;
#endif
			num = (size_t)(e - args->argv[i]);
			a = xstrndup(args->argv[i], num);
			args_insert_string(args, i, a);
			free(a);
			strpull(args->argv[i+1], num);
			break;
		}
	}
	args_remove_empty(args);
	return r;
}

/* rearranges redirections characters (|, >, <) so
 * they appear in separate argsuments
 */
int rearrange_redirections(args_t *args)
{
	return rearrange_args(args, "|><");
}

/* given an alias expanded args_t, ARGS, which may include
 * %-parameters (%*, %1, %2, ...), expand those parameters
 * using the arguments in alias_args
 *
 * redirection stuff is not included in the %-parameters
 *
 * alias_args is modified (possibly cleared)
 *
 * modifies ARGS
 */

void expand_alias_parameters(args_t **args, args_t *alias_args)
{
	int i;
	args_t *new_args;
	args_t *redir_args = 0;
	args_t *deleted;

/*	if(alias_args->argc == 0)
		return;*/

	rearrange_args(*args, ";");

	/* do not include redirections in %-parameters */
	i = rearrange_redirections(alias_args);
	if(i != -1) {
		redir_args = args_create();
		args_add_args2(redir_args, alias_args, i);
		args_del(alias_args, i, alias_args->argc - i);
	}

	deleted = args_create();
	args_add_args(deleted, alias_args);

	new_args = args_create();
	args_add_args(new_args, *args);

	for(i=0; i<new_args->argc; i++)
	{
		char *e;

		/* %* can not be used embedded in an argument */
		if(strcmp(new_args->argv[i], "%*") == 0) {
			args_del(new_args, i, 1);
			args_insert_args(new_args, i, deleted, 0, deleted->argc);
			args_clear(alias_args);
			continue;
		}

		e = strqchr(new_args->argv[i], '%');
		if(e) {
			char *ep;
			char *ins;
			char *tmp;
			int n;

			*e = 0;
			n = strtoul(++e, &ep, 0) - 1;

			if(ep != e && n < alias_args->argc && n >= 0) {
				ins = deleted->argv[n];
				free(alias_args->argv[n]);
				alias_args->argv[n] = 0;
			} else
				ins = 0;

			/* insert the parameter in this argument */
			asprintf(&tmp, "%s%s%s", new_args->argv[i], ins ? ins : "", ep);
			free(new_args->argv[i]);
			new_args->argv[i] = tmp;
		}

	}

	args_destroy(deleted);
	args_remove_empty(alias_args);
	args_add_args(new_args, alias_args);

	if(redir_args) {
		args_add_args(new_args, redir_args);
		args_destroy(redir_args);
	}

	args_remove_empty(new_args);

	args_destroy(*args);
	*args = new_args;
}

cmd_t *find_func(const char *cmd, bool print_error)
{
	cmd_t *c;
	alias *a;

	/* finds (abbreviated) command */
	c = find_cmd(cmd);

	/* finds (abbreviated) alias */
	a = alias_search(cmd);

	if(!c && !a) {
		if(print_error)
			fprintf(stderr, _("No such command '%s', try 'help'\n"), cmd);
		return 0;
	}

	if(a == ALIAS_AMBIGUOUS) {
		/* skip alias if exact command */
		if(c && c != CMD_AMBIGUOUS && strlen(c->cmd) == strlen(cmd))
			return c;
		else {
			if(print_error)
				fprintf(stderr, _("ambiguous alias '%s'\n"), cmd);
			return 0;
		}
	}

	if(c == CMD_AMBIGUOUS) {
		if(!a) {
			if(print_error)
				fprintf(stderr, _("ambiguous command '%s'\n"), cmd);
			return 0;
		}
		/* skip ambiguous command, run exact alias instead */
		c = 0;
	}

	if(c && !a)
		return c;

	/* found both alias and command */
	if(a && c) {
		if(strlen(a->name) == strlen(cmd))
			/* skip exact command, run exact alias */
			c = 0;
		else if(strlen(c->cmd) == strlen(cmd))
			/* skip alias, run exact command */
			return c;
		else {
			/* both alias and command are abbreviated
			 * Q: which wins? A: no one!?
			 */
			if(print_error)
				fprintf(stderr, _("ambiguous alias/command '%s'\n"), cmd);
			return 0;
		}
	}
	/* now we should have the alias only */

	c = find_cmd(a->value->argv[0]);
	if(!c) {
		if(print_error)
			fprintf(stderr, _("No such command '%s' (expanded alias '%s')\n"),
				   a->value->argv[0], a->name);
		return 0;
	}
	if(c == CMD_AMBIGUOUS) {
		if(print_error)
			fprintf(stderr,
					_("ambiguous command '%s' (expanded alias '%s')\n"),
				   a->value->argv[0], a->name);
		return 0;
	}

/*	expand_alias_parameters(args, a->value);*/

	return c;
}

void cmd_list(int argc, char **argv)
{
	OPT_HELP("List files.  Usage:\n"
			 "  list [options] [file]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");
	need_connected();
	need_loggedin();

	if(argc == optind + 1)
		ftp_list("LIST", 0, stdout);
	else {
		char *args;

		args = args_cat(argc, argv, optind);
		ftp_list("LIST", args, stdout);
		free(args);
	}
}

void cmd_nlist(int argc, char **argv)
{
	OPT_HELP("Simple file list.  Usage:\n"
			 "  nlist [options] [file]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");
	need_connected();
	need_loggedin();

	if(argc == optind + 1)
		ftp_list("NLST", 0, stdout);
	else {
		char *args;

		args = args_cat(argc, argv, optind);
		ftp_list("NLST", args, stdout);
		free(args);
	}
}

void cmd_cat(int argc, char **argv)
{
	int i;
	struct option longopts[] = {
		{"type", required_argument, 0, 't'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int c;
	transfer_mode_t mode = tmAscii;

	optind = 0;
	while((c = getopt_long(argc, argv, "t:h", longopts, 0)) != EOF) {
		switch(c) {
		case 't':
			if(strncmp(optarg, "ascii", strlen(optarg)) == 0)
				mode = tmAscii;
			else if(strncmp(optarg, "binary", strlen(optarg)) == 0)
				mode = tmBinary;
			else {
				fprintf(stderr,
						_("Invalid option argument --type=%s\n"), optarg);
				return;
			}
			break;
		case 'h':
			fprintf(stderr, "Print file(s) on standard output.  Usage:\n"
					"  cat [options] <file>...\n"
					"Options:\n"
					"  -t, --type=TYPE    set transfer TYPE to ascii"
					" or binary\n"
					"  -h, --help         show this help\n");
			return;
		case '?':
			optind = -1;
			return;
		}
	}

	minargs(optind);
	need_connected();
	need_loggedin();

	for(i = optind; i < argc; i++) {
		listitem *gli;
		list *gl = rglob_create();
		stripslash(argv[i]);
		if(rglob_glob(gl, argv[i], true, false, 0) == -1)
			fprintf(stderr, _("%s: no matches found\n"), argv[i]);
		for(gli = gl->first; gli; gli=gli->next) {
			rfile *rf = (rfile *)gli->data;
			const char *fn = base_name_ptr(rf->path);
			if(strcmp(fn, ".") != 0 && strcmp(fn, "..") != 0) {
				ftp_receive(rf->path, stdout, mode, 0);
				fflush(stdout);
			}
		}
		rglob_destroy(gl);
	}
}

void cmd_cd(int argc, char **argv)
{
	char *e;
#if 0
	OPT_HELP("Change working directory.  Usage:\n"
			 "  cd [options] [directory]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n"
			 "if [directory] is '-', cd changes to the previous working"
			 " directory\n"
			 "if omitted, changes to home directory\n");

	maxargs(optind);
#endif

	maxargs_nohelp(1);
	need_connected();
	need_loggedin();

	if(argc <= 1)
		e = ftp->homedir;
	else if(strcmp(argv[1], "-") == 0) {
		e = ftp->prevdir;
		if(e == 0) /* no previous directory */
			return;
	} else
		e = argv[1];
	e = tilde_expand_home(e, ftp->homedir);
	ftp_chdir(e);
	free(e);
}

void cmd_cdup(int argc, char **argv)
{
	OPT_HELP("Change to parent working directory.  Usage:\n"
			 "  cdup [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");
	maxargs(optind - 1);
	need_connected();
	need_loggedin();
	ftp_cdup();
}

void opt_help(int argc, char **argv, char *help)
{
	struct option longopts[] = {
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int c;

	optind = 0;
	while((c = getopt_long(argc, argv, "h", longopts, 0)) != EOF) {
		switch(c) {
		  case 'h':
			fprintf(stderr, "%s", help);
		  case '?':
			optind = -1;
			return;
		}
	}
}

void cmd_pwd(int argc, char **argv)
{
	OPT_HELP("Print the current working directory.  Usage:\n"
			 "  pwd [options]\n"
			 "Options:\n"
			 "  -h, --help     show this help\n");

	maxargs(optind - 1);
	need_connected();
	need_loggedin();

	ftp_pwd();
}

void cmd_url(int argc, char **argv)
{
	int c;
	struct option longopts[] = {
		{"no-encoding", no_argument, 0, 'e'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	bool no_encoding = false;

	optind = 0;
	while((c = getopt_long(argc, argv, "eh", longopts, 0)) != EOF) {
		switch(c) {
		  case 'h':
			printf(_("Print the current URL.  Usage:\n"
					 "  url [options]\n"
					 "Options:\n"
					 "  -e, --no-encoding    don't encode URL as"
					 " rfc1738 says\n"
					 "  -h, --help           show this help\n"));
			return;
		  case 'e':
			no_encoding = true;
			break;
		  case '?':
			return;
		}
	}

	maxargs(optind - 1);
	need_connected();
	need_loggedin();

	printf("ftp://");

	if(no_encoding) {
		printf("%s@%s", ftp->url->username, ftp->url->hostname);
		if(ftp->url->port != 21)
			printf(":%d", ftp->url->port);
		printf("/%s\n", ftp->curdir);
		return;
	}

	if(!url_isanon(ftp->url)) {
		char *e = encode_url_username(ftp->url->username);
		printf("%s@", e);
		free(e);
	}
	printf("%s", ftp->url->hostname);
	if(ftp->url->port != 21)
		printf(":%u", ftp->url->port);
	if(strcmp(ftp->curdir, ftp->homedir) != 0) {
		char *d;
		char *e = ftp->curdir;
#if 0
		if(strncmp(ftp->curdir, ftp->homedir, strlen(ftp->homedir)) == 0)
			e = ftp->curdir + strlen(ftp->homedir) + 1;
#endif
		d = encode_url_directory(e);
		printf("/%s", d);
		free(d);
	}
	printf("\n");
}

void cmd_close(int argc, char **argv)
{
	if(argv) {
		OPT_HELP("Close open connection.  Usage:\n"
				 "  close [options]\n"
				 "Options:\n"
				 "  -h, --help     show this help\n");
		maxargs(optind - 1);
	}

	ftp_quit();
	if(list_numitem(gvFtpList) > 1) {
		list_delitem(gvFtpList, gvCurrentFtp);
		gvCurrentFtp = gvFtpList->first;
		ftp_use((Ftp *)gvCurrentFtp->data);
	}
}

void cmd_rhelp(int argc, char **argv)
{
	char *e;

	e = args_cat(argc, argv, 1);
	ftp_help(e);
	free(e);
}

void cmd_mkdir(int argc, char **argv)
{
	int i;
	OPT_HELP("Create directory.  Usage:\n"
			 "  mkdir [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");
	minargs(optind);
	need_connected();
	need_loggedin();

	for(i=optind; i<argc; i++)
		ftp_mkdir(argv[i]);
}

void cmd_rmdir(int argc, char **argv)
{
	int i;
	OPT_HELP("Remove directory.  Usage:\n"
			 "  rmdir [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");
	minargs(optind);
	need_connected();
	need_loggedin();

	for(i=optind; i<argc; i++)
		ftp_rmdir(argv[i]);
}

void cmd_idle(int argc, char **argv)
{
	OPT_HELP("Get or set idle timeout.  Usage:\n"
			 "  idle [options] [timeout]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n"
			 "Without the timeout option, print the current idle timeout\n");
	maxargs(optind);
	need_connected();
	need_loggedin();

	if(argc - 1 == optind)
		ftp_idle(argv[optind]);
	else
		ftp_idle(0);
}

void cmd_nop(int argc, char **argv)
{
	OPT_HELP("Do nothing (send a NOOP command).  Usage:\n"
			 "  nop [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");
	maxargs(optind - 1);
	need_connected();

	ftp_noop();
}

void cmd_rstatus(int argc, char **argv)
{
	OPT_HELP("Show status of remote host.  Usage:\n"
			 "  rstatus [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	maxargs(optind - 1);
	need_connected();

	if(ftp->ssh_pid) {
		printf("No status for SSH connection\n");
		return;
	}

	ftp_set_tmp_verbosity(vbCommand);
	ftp_cmd("STAT");
}

void cmd_status(int argc, char **argv)
{
	OPT_HELP("Show status.  Usage:\n"
			 "  status [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	maxargs(optind - 1);

	if(ftp_connected())
		printf(_("connected to '%s'\n"), ftp->host->ohostname);
	else
		puts(_("not connected"));
	if(ftp_loggedin())
		printf(_("logged in as '%s'\n"), ftp->url->username);
#ifdef HAVE_KERBEROS
	if(ftp_connected())
		sec_status();
#endif
	if(list_numitem(gvFtpList) > 1 || ftp_connected())
		printf(_("There are totally %u connections open\n"),
			   list_numitem(gvFtpList));
}

void cmd_site(int argc, char **argv)
{
	char *e;

	minargs_nohelp(1);
	need_connected();

	if(ftp->ssh_pid) {
		printf("SITE commands not available in SSH connections\n");
		return;
	}

	e = args_cat(argc, argv, 1);
	ftp_set_tmp_verbosity(vbCommand);
	ftp_cmd("SITE %s", e);
	free(e);
}

void cmd_chmod(int argc, char **argv)
{
	int i;
	list *gl;
	listitem *li;

	OPT_HELP("Change permissions on remote file.  Usage:\n"
			 "  chmod [options] <mode> <file>\n"
			 "Options:\n"
			 "  -h, --help    show this help\n"
			 "<mode> is the permission mode, in octal (ex 644)\n");

	minargs(optind + 1);
	need_connected();
	need_loggedin();

	gl = rglob_create();
	for(i=optind+1; i<argc; i++) {
		stripslash(argv[i]);
		rglob_glob(gl, argv[i], true, true, NODOTDIRS);
	}

	for(li=gl->first; li; li=li->next) {
		if(ftp_chmod(((rfile *)li->data)->path, argv[optind]) != 0)
			printf("%s: %s\n", ((rfile *)li->data)->path, ftp_getreply(false));
	}
	rglob_destroy(gl);
}

void cmd_mv(int argc, char **argv)
{
	OPT_HELP("Rename or move a file.  Usage:\n"
			 "  mv [options] <src> <dest>\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(optind + 1);
	maxargs(optind + 1);
	need_connected();
	need_loggedin();

	ftp_set_tmp_verbosity(vbError);
	if(ftp_rename(argv[optind], argv[optind + 1]) == 0)
		printf("%s -> %s\n", argv[optind], argv[optind + 1]);
}

void cmd_cache(int argc, char **argv)
{
	int c;
	struct option longopts[] = {
		{"clear", no_argument, 0, 'c'},
		{"list", no_argument, 0, 'l'},
		{"touch", no_argument, 0, 't'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	bool touch = false;

	optind = 0;
	while((c = getopt_long(argc, argv, "clt::h", longopts, 0)) != EOF) {
		switch(c) {
		  case 'c':
			ftp_cache_clear();
			return;
		  case 'l':
			ftp_cache_list_contents();
			return;
		  case 't':
			  touch = true;
			  break;
		  case 'h':
			printf(_("Control the directory cache.  Usage:\n"
					 "  cache [option] [directories]\n"
					 "Options:\n"
					 "  -c, --clear        clear whole directory cache\n"
					 "  -l, --list         list contents of cache\n"
					 "  -t, --touch        remove directories from cache\n"
					 "                     if none given, remove current"
					 " directory\n"
					 "  -h, --help         show this help\n"));
			return;
		  case '?':
			return;
		}
	}

	need_connected();
	need_loggedin();

	if(touch) {
		if(optind < argc) {
			int i;
			for(i = optind; i < argc; i++)
				ftp_cache_flush_mark(argv[i]);
		} else
			ftp_cache_flush_mark(ftp->curdir);
	} else {
		minargs(1);
		maxargs(0);
	}
}

void cmd_quote(int argc, char **argv)
{
	char *e;

	OPT_HELP("Send arbitrary FTP command.  Usage:\n"
			 "  quote [options] <commands>\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(optind);
	need_connected();

	if(ftp->ssh_pid) {
		printf("Command not available in SSH connection\n");
		return;
	}

	e = args_cat(argc, argv, optind);
	ftp_set_tmp_verbosity(vbDebug);
	ftp_cmd("%s", e);
	free(e);
}

void cmd_filetime(int argc, char **argv)
{
	int i;

	OPT_HELP("Show modification time of remote file.  Usage:\n"
			 "  filetime [options] <file>...\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(optind);
	need_connected();
	need_loggedin();

	for(i=optind;i<argc;i++) {
		time_t t = ftp_filetime(argv[i]);
		if(t != (time_t) -1)
			printf("%s: %s", argv[i], ctime(&t));
		else
			printf("%s\n", ftp_getreply(false));
	}
}

/* in rc.c */
int parse_rc(const char *file, bool warn);

/* in main.c */
void init_ftp(void);

void cmd_source(int argc, char **argv)
{
	int i;

	OPT_HELP("Read (source) a configuration file.  Usage:\n"
			 "  source [options] <file>...\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(optind);

	for(i = optind; i < argc; i++)
		parse_rc(argv[i], true);
	init_ftp();
}

void cmd_system(int argc, char **argv)
{
	if(argv) {
		OPT_HELP("Show type of remote system.  Usage:\n"
				 "  system [options]\n"
				 "Options:\n"
				 "  -h, --help    show this help\n");

		maxargs(optind - 1);
	}
	need_connected();
	need_loggedin();

	if(ftp->ssh_pid) {
		fprintf(stderr, "remote system: SSH (version %d)\n", ftp->ssh_version);
		return;
	}

	if(ftp_get_verbosity() != vbDebug)
		fprintf(stderr, _("remote system: "));
	ftp_set_tmp_verbosity(vbCommand);
	ftp_cmd("SYST");
}

void cmd_switch(int argc, char **argv)
{
	OPT_HELP("Switch between active connections.  Usage:\n"
			 "  switch [options] [number | name]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n"
			 "The argument can either be the connection number, host name"
			 " or its alias\n"
			 "Without argument, switch to the next active connection\n");

	maxargs(optind);

	if(argc > optind) {
		listitem *tmp = ftplist_search(argv[optind]);
		if(tmp)
			gvCurrentFtp = tmp;
	} else {
		if(gvCurrentFtp == gvFtpList->last)
			gvCurrentFtp = gvFtpList->first;
		else {
			if(gvCurrentFtp->next)
				gvCurrentFtp = gvCurrentFtp->next;
		}
	}

	ftp_use((Ftp *)gvCurrentFtp->data);
}

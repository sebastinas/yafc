/* $Id: cmd.c,v 1.12 2004/05/20 11:10:52 mhe Exp $
 *
 * cmd.c -- read and execute commands, this is the main loop
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
#include "gvars.h"
#include "ftp.h"
#include "cmd.h"
#include "input.h"
#include "strq.h"
#include "args.h"
#include "redir.h"
#include "transfer.h"
#include "commands.h"
#include "alias.h"
#include "utils.h"
#include "bookmark.h"
#include "redir.h"
#include "prompt.h"
#include "ltag.h"

//static void exe_cmdline(char *str, bool aliases_are_expanded);

void exit_yafc(void)
{
	ftp_quit_all();
	list_free(gvFtpList);
	input_save_history();
	save_ltaglist(0);
	gvars_destroy();
	reset_xterm_title();
	exit(0);
}

void cmd_quit(int argc, char **argv)
{
	if(argv != 0) {
		OPT_HELP("Close all connections and quit.  Usage:\n"
				 "  quit [options]\n"
				 "Options:\n"
				 "  -h, --help    show this help\n");

		maxargs(optind - 1);
	}
	exit_yafc();
}

/* main loop, prompts for commands and executes them.
 */
void command_loop(void)
{
	char *p;
#ifdef HAVE_GETTIMEOFDAY
	struct timeval beg, end;
#endif

#ifdef HAVE_POSIX_SIGSETJMP
	if(sigsetjmp(gvRestartJmp, 1))
#else
	if(setjmp(gvRestartJmp))
#endif
	{
		if(!ftp_connected())
			printf(_("restarted command loop, connection closed\n"));
		else
			printf(_("restarted command loop, command aborted\n"));
	}
	gvJmpBufSet = true;
	force_completion_type = cpUnset;
	close_redirection();
	gvInTransfer = false;
	gvInterrupted = false;

    while(!gvSighupReceived) {
		char *cmdstr, *s;

		ftp_initsigs();

#if 0 && (defined(HAVE_SETPROCTITLE) || defined(linux))
		if(gvUseEnvString) {
			if(ftp_connected())
				setproctitle("%s", ftp->url->hostname);
			else
				setproctitle(_("not connected"));
		}
#endif

		fputc('\r', stderr);
		p = expand_prompt(ftp_connected()
						  ? (ftp_loggedin() ? gvPrompt3 : gvPrompt2)
						  : gvPrompt1);
		if(!p)
			p = xstrdup("yafc> ");

		print_xterm_title();

		cmdstr = input_read_string(p);
		free(p);

		if(!cmdstr) {  /* bare EOF received */
			fputc('\n', stderr);
			if(gvQuitOnEOF)
				break;
			else
				continue;
		}
		s = strip_blanks(cmdstr);
		if(!*s) {
			/* blank line */
			free(cmdstr);
			continue;
		}
#ifdef HAVE_LIBREADLINE
		add_history(s);
#endif

#ifdef HAVE_GETTIMEOFDAY
		gettimeofday(&beg, 0);
#endif
		ftp_trace("yafc: '%s'\n", s);
		exe_cmdline(s, false);
		free(cmdstr);

#ifdef HAVE_GETTIMEOFDAY
		gettimeofday(&end, 0);
		end.tv_sec -= beg.tv_sec;
		if(gvBeepLongCommand && end.tv_sec >= gvLongCommandTime)
			fputc('\007', stderr);
#endif
    }
	/* end of main loop, exiting program... */
	if(gvSighupReceived)
		transfer_end_nohup();
	else
		cmd_quit(0, 0);
}

static void exe_cmd(cmd_t *c, args_t *args)
{
	int i;
	char *e;

	if(!ftp_connected() && c->needconnect)
		fprintf(stderr,
				_("Not connected. Try 'open --help' for more information.\n"));
	else if(!ftp_loggedin() && c->needlogdin)
		fprintf(stderr,
				_("Not logged in. Try 'user --help' for more information.\n"));
	else {
		for(i=1; i<args->argc; i++) {
			int ret;
			switch(args->argv[i][0]) {
			  case '|':
			  case '>':
				e = args_cat2(args, i);
				/* remove pipe/redir from command parameters */
				args_del(args, i, args->argc);
				ret = open_redirection(e); /* modifies stdout and/or stderr */
				free(e);
				if(ret != 0)
					return;
				break;
			  case '<':
				fprintf(stderr, _("input redirection not supported\n"));
				return;
			}
		}

		if(c->auto_unquote)
			args_unquote(args);
		args_remove_empty(args);

		if(strcmp(args->argv[0], "shell") == 0
		   || strcmp(args->argv[0], "!") == 0)
		{
			char *e = args_cat(args->argc, args->argv, 1);
			bool b = reject_ampersand(e);
			free(e);
			if(b)
				return;
		}

		gvInterrupted = false;
		c->func(args->argc, args->argv);
		gvInterrupted = false;
		gvInTransfer = false;
		close_redirection();
		ftp_cache_flush();
	}
}

static args_t *expand_alias(const char *cmd)
{
	args_t *args;
	alias *a;

	a = alias_search(cmd);
	if(a == 0 || a == ALIAS_AMBIGUOUS)
		return (args_t *)a;

	args = args_create();
	args_add_args(args, a->value);

	return args;
}

/* executes the commandline in STR
 * handles expansion of aliases and splits the command line
 * into ;-separated strings which are fed to exe_cmd()
 *
 * modifies STR
 *
 * first call should pass aliases_are_expanded == false
 */
void exe_cmdline(char *str, bool aliases_are_expanded)
{
	char *e;

/*	fprintf(stderr, "exe_cmdline: %s\n", str);*/

	/* split command into ;-separated commands */
	while((e = strqsep(&str, ';')) != 0) {
		args_t *args;

		/* make an args_t of the command string */
		args = args_create();
		args_push_back(args, e);

		/* remove empty arguments */
		args_remove_empty(args);
		if(args->argc == 0) {
			args_destroy(args);
		   continue;
		}

		rearrange_redirections(args);

		if(aliases_are_expanded) {
			cmd_t *c;

			/* special handling of '!' (synonym for shell command) */
			if(args->argv[0][0] == '!' && strlen(args->argv[0]) > 1) {
				strpull(args->argv[0], 1);
				args_push_front(args, "!");
			}

			/* now we have the expanded command line in args
			 * this string might include multiple real commands (;-separated)
			 */

			rearrange_redirections(args);

			c = find_func(args->argv[0], true);

			if(c != 0)
				exe_cmd(c, args);
		} else {
			args_t *expanded_cmd;
			char *cmd;
			char *xstr;

			/* get the command, the first word in the string */
			cmd = xstrdup(args->argv[0]);
			unquote(cmd);

			/* expand command if it's an alias */
			expanded_cmd = expand_alias(cmd);

			if(expanded_cmd == 0) /* it wasn't an alias */
				expanded_cmd = args;
			else if(expanded_cmd == (args_t *)ALIAS_AMBIGUOUS) {
				fprintf(stderr, _("ambiguous alias '%s'\n"), cmd);
				args_destroy(args);
				free(cmd);
				continue;
			} else {
				args_t *alias_args;

				/* get the arguments for the alias
				 * these are used with expand_alias_parameters()
				 */
				alias_args = args_create();
				args_add_args2(alias_args, args, 1);

				expand_alias_parameters(&expanded_cmd, alias_args);
				args_destroy(alias_args);
				args_destroy(args);

				/* remove empty arguments */
				args_remove_empty(expanded_cmd);
				if(expanded_cmd->argc == 0) {
					args_destroy(expanded_cmd);
					continue;
				}
			}

			free(cmd);

			xstr = args_cat2(expanded_cmd, 0);
			exe_cmdline(xstr, true);
			free(xstr);
			args_destroy(expanded_cmd);
		}
	}
}

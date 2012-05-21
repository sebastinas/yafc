/*
 * main.c -- parses command line options and starts Yafc
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
#include "ftp.h"
#include "gvars.h"
#include "help.h"
#include "alias.h"
#include "input.h"
#include "cmd.h"
#if 0
# include "setproctitle.h"
#endif
#include "completion.h"
#include "login.h"
#include "strq.h"
#include "utils.h"
#include "rc.h"
#include "ltag.h"
#include "lscolors.h"

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "yafcrc.h"

void print_syntax_and_exit(char *argv0)
{
	printf(COPYLINE "\n");
	printf(_("This is yet another FTP client.  Usage:\n"
			 " %s [options] [[proto://][user[:password]@]hostname[:port][/directory] ...]\n"
			 " proto can be either 'ftp' or 'ssh'\n"
			 "Options:\n"
			 "  -a, --anon        (*) anonymous login\n"
			 "  -d, --debug           print all commands sent to/from server\n"
			 "  -D, --dump-rc         prints the default config file to stdout\n"
			 "  -m, --mechanism=MECH\n"
			 "                    (*) try MECH as security mechanism(s)\n"
			 "  -n, --norc            don't parse config file\n"
			 "  -p, --noproxy     (*) don't connect via proxy\n"
			 "  -q, --quiet           don't print the yafc welcome message\n"
			 "  -r, --rcfile=FILE     use other config file instead of ~/.yafc/yafcrc\n"
			 "  -t, --trace[=FILE]    use a trace file (mainly for debugging)\n"
			 "               if FILE specified, use it instead of ~/.yafc/trace/trace.<pid>\n"
			 "  -u, --noauto      (*) disable autologin\n"
			 "  -U, --noalias     (*) disable bookmark alias lookup and abbreviation\n"
			 "  -v, --verbose         print all replies from server\n"
			 "  -w, --wait=TIME       use a different wait time for reconnecting\n"
			 "  -W, --workdir=DIR     use a different working directory (instead of ~/.yafc)\n"
			 "  -V, --version         print version information and quit\n"
			 "  -h, --help            print this help and quit\n"
			 "\n"
			 "(*) only applies for login to host specified on the command line\n"
			 "\n"
			 "Report bugs to " PACKAGE_BUGREPORT "\n\n"), argv0);
	exit(0);
}

void tstp_sighandler(int signum)
{
	if(signum == SIGTSTP) {
		reset_xterm_title();
		raise(SIGSTOP);
	} else if(signum == SIGCONT) {
		if(readline_running)
			input_redisplay_prompt();
		print_xterm_title();
	}
	ftp_set_signal(signum, tstp_sighandler);
}

void init_ftp(void)
{
	ftp->getuser_hook = getuser_hook;
	ftp->getpass_hook = getpass_hook;

	ftp->open_timeout = gvConnectionTimeout;
	ftp->reply_timeout = gvCommandTimeout;

	ftp_set_verbosity(vbError); /* default */
	if(gvVerbose)
		ftp_set_verbosity(vbCommand);
	if(gvDebug)
		ftp_set_verbosity(vbDebug);
}

void init_yafc(void)
{
#ifdef HAVE_UNAME
	struct utsname unbuf;
#endif

	gvFtpList = list_new((listfunc)ftp_destroy);
	list_additem(gvFtpList, ftp_create());
	gvCurrentFtp = gvFtpList->first;
	ftp_use((Ftp *)gvCurrentFtp->data);
	ftp_initsigs();

	gvEditor = getenv("EDITOR");
	if(!gvEditor)
		gvEditor = getenv("VISUAL");
	if(gvEditor)
		gvEditor = xstrdup(gvEditor);
	else
		gvEditor = xstrdup("vi");

#ifdef HAVE_UNAME
	if(uname(&unbuf) == 0)
		gvLocalHost = xstrdup(unbuf.nodename);
#endif
	if(!gvLocalHost)
		gvLocalHost = xstrdup(getenv("HOST"));
	if(!gvLocalHost)
		gvLocalHost = xstrdup("localhost");

	{
		struct passwd *pwd;
		pwd = getpwuid(geteuid());
		gvUsername = xstrdup(pwd->pw_name);
		gvLocalHomeDir = xstrdup(pwd->pw_dir);
	}

	if(!gvLocalHomeDir)
		gvLocalHomeDir = xstrdup(getenv("HOME"));

	gvWorkingDirectory = path_absolute(
		gvWorkingDirectory ? gvWorkingDirectory : "~/.yafc",
		get_local_curdir(),
		gvLocalHomeDir);

	gvAnonPasswd = xstrdup("anonymous@");

	/* init colors from LS_COLORS for ls */
	init_colors();

	/* choose default security mechanism */
	gvDefaultMechanism = list_new((listfunc)free);
#ifdef HAVE_KRB4
# ifdef HAVE_KRB5
	listify_string("krb5:krb4:none", gvDefaultMechanism);
# elif defined(USE_SSL)
	listify_string("krb5:krb4:ssl:none", gvDefaultMechanism);
# else
	listify_string("krb4:none", gvDefaultMechanism);
# endif
#elif defined(HAVE_KRB5)
# ifdef USE_SSL
	listify_string("krb5:ssl:none", gvDefaultMechanism);
# else
	listify_string("krb5:none", gvDefaultMechanism);
# endif
#elif defined(USE_SSL)
	listify_string("ssl", gvDefaultMechanism);
#else
	listify_string("none", gvDefaultMechanism);
#endif

	gvPrompt1 = xstrdup("yafc> ");
	gvPrompt2 = xstrdup("yafc %h> ");
	gvPrompt3 = xstrdup("yafc %h:%42~> ");

	gvTerm = xstrdup(getenv("TERM"));
	if(!gvTerm)
		gvTerm = xstrdup("dummy");
	gvXtermTitleTerms = xstrdup("xterm xterm-debian rxvt");

	gvBookmarks = list_new((listfunc)url_destroy);
	gvAliases = list_new((listfunc)alias_destroy);
	gvAsciiMasks = list_new((listfunc)free);
	gvTransferFirstMasks = list_new((listfunc)free);
	gvIgnoreMasks = list_new((listfunc)free);
	gvLocalTagList = list_new((listfunc)free);
	gvProxyExclude = list_new((listfunc)free);

	asprintf(&gvHistoryFile, "%s/history", gvWorkingDirectory);

	gvSendmailPath = xstrdup("/usr/sbin/sendmail");

	ftp_set_signal(SIGTSTP, tstp_sighandler);
	ftp_set_signal(SIGCONT, tstp_sighandler);

	gvTransferBeginString = xstrdup("%-70R\n");
	gvTransferString = xstrdup("%5p%% [%25v] %s/%S ETA %e %B");
/*	gvTransferEndString = xstrdup("%-40R      %s in %t @ %b\n");*/

	gvTransferXtermString = xstrdup("\x1B]0;yafc - (%p%%) %r\x07");

	gvStatsTransfer = stats_create();

	input_init();
}

void check_if_first_time(void)
{
	if(access(gvWorkingDirectory, X_OK) == 0)
		return;

	if(errno == ENOENT) {
		char *dir;

/*		printf(_("This seems to be the first time you run Yafc...\n"));*/

		printf(_("creating working directory %s: "), gvWorkingDirectory);
		fflush(stdout);
		if(mkdir(gvWorkingDirectory, S_IRUSR|S_IWUSR|S_IXUSR) != 0) {
			perror("");
			return;
		}
		chmod(gvWorkingDirectory, S_IRUSR|S_IWUSR|S_IXUSR);
		printf(_("done\n"));

		asprintf(&dir, "%s/trace", gvWorkingDirectory);
		printf(_("creating directory %s: "), dir);
		fflush(stdout);
		if(mkdir(dir, S_IRUSR|S_IWUSR|S_IXUSR) != 0) {
			perror("");
			free(dir);
			return;
		}
		chmod(dir, S_IRUSR|S_IWUSR|S_IXUSR);
		printf(_("done\n"));
		free(dir);

		asprintf(&dir, "%s/nohup", gvWorkingDirectory);
		printf(_("creating directory %s: "), dir);
		fflush(stdout);
		if(mkdir(dir, S_IRUSR|S_IWUSR|S_IXUSR) != 0) {
			perror("");
			free(dir);
			return;
		}
		chmod(dir, S_IRUSR|S_IWUSR|S_IXUSR);
		printf(_("done\n"));
		free(dir);

#if 0
		else {
			printf(_("creating default config file /yafcrc: "),
				   gvWorkingDirectory);
			fflush(stdout);
			{
				FILE *fp;
				fp = fopen(, "w");
				if(!fp) {
					perror("");
					free(rcdir);
					return;
				}
				fprintf(fp, "%s", default_yafcrc);
				fclose(fp);
				printf(_("done\n"));
			}
		}
#endif
	} else
		perror(gvWorkingDirectory);
}

int main(int argc, char **argv, char **envp)
{
	int c;
	char *configfile = 0;
	bool dotrace = false;
	char *tracefile = 0;
	unsigned int open_opt = 0;
	int wait_time = -1;

	bool override_yafcrc = false;
	bool override_debug = false;
	bool override_verbose = false;
	bool override_welcome = false;

	char *mech = 0;

	struct option longopts[] = {
		{"anon", no_argument, 0, 'a'},
		{"debug", no_argument, 0, 'd'},
		{"dump-rc", no_argument, 0, 'D'},
		{"norc", no_argument, 0, 'n'},
		{"quiet", no_argument, 0, 'q'},
		{"rcfile", required_argument, 0, 'r'},
		{"mechanism", required_argument, 0, 'm'},
		{"noproxy", no_argument, 0, 'p'},
		{"trace", optional_argument, 0, 't'},
		{"noauto", no_argument, 0, 'u'},
		{"verbose", no_argument, 0, 'v'},
		{"version", no_argument, 0, 'V'},
		{"wait", required_argument, 0, 'w'},
		{"workdir", required_argument, 0, 'W'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0},
	};

#ifdef SOCKS
	SOCKSinit(argv[0]);
#endif

#ifdef ENABLE_NLS
# ifdef HAVE_SETLOCALE
	setlocale(LC_ALL, "");
# endif
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif
#if 0 && (!defined(HAVE_SETPROCTITLE) && defined(linux))
	initsetproctitle(argc, argv, envp);
#endif

	while((c = getopt_long(argc, argv,
						   "qhdDgant::r:uUm:pvVw:W:", longopts, 0)) != EOF)
	{
		switch(c) {
		case 'a':
			open_opt |= OP_ANON;
			break;
		case 'n':
			override_yafcrc = true;
			break;
		case 'r':
			configfile = xstrdup(optarg);
			break;
		case 't':
			dotrace = true;
			if(optarg)
				tracefile = xstrdup(optarg);
			break;
		case 'u':
			open_opt |= OP_NOAUTO;
			break;
		case 'U':
			open_opt |= OP_NOALIAS;
			break;
		case 'm':
			mech = xstrdup(optarg);
			break;
		case 'p':
			open_opt |= OP_NOPROXY;
			break;
		case 'd':
			override_debug = true;
			break;
		case 'D':
			printf("%s", default_yafcrc);
			return 0;
		case 'q':
			override_welcome = true;
			break;
		case 'v':
			override_verbose = true;
			break;
		case 'V':
			printf(PACKAGE " " VERSION " (" HOSTTYPE ")\n");
			return 0;
		case 'w':
			wait_time = atoi(optarg);
			break;
		case 'W':
			stripslash(optarg);
			gvWorkingDirectory = xstrdup(optarg);
			break;
		case 'h':
			print_syntax_and_exit(argv[0]);
			break;
		case '?':
			return 1;
		}
	}

	if(!override_welcome)
		puts(_(FULLVER));

	init_yafc();

	if(!configfile)
		asprintf(&configfile, "%s/yafcrc", gvWorkingDirectory);

	check_if_first_time();

	if(!override_yafcrc) {
		char *tmp;
		parse_rc(SYSCONFDIR "/yafcrc", false);
		parse_rc(configfile, false);
		asprintf(&tmp, "%s/bookmarks", gvWorkingDirectory);
		parse_rc(tmp, false);
		free(tmp);
	}

	if(gvReadNetrc)
		parse_rc("~/.netrc", false);

	if(gvProxyType != 0 && gvProxyUrl == 0) {
		fprintf(stderr, _("No proxy host defined!\n\n"));
		gvProxyType = 0;
	}

	if(override_debug)
		gvDebug = true;
	if(override_verbose)
		gvVerbose = true;
	if(wait_time != -1) {
		if(wait_time < 0)
			fprintf(stderr, _("Invalid value for --wait: %d\n"), wait_time);
		else
			gvConnectWaitTime = wait_time;
	}

	init_ftp();

	if(dotrace || gvTrace) {
		if(!tracefile)
			asprintf(&tracefile, "%s/trace/trace.%u",
					 gvWorkingDirectory, getpid());
		if(ftp_set_trace(tracefile) != 0)
			fprintf(stderr, _("Couldn't open tracefile '%s': %s\n"),
					tracefile, strerror(errno));
		free(tracefile);
	}

	if(optind < argc) {
#ifdef HAVE_LIBREADLINE
		char *s;
#endif
		int i;

		if(!gvAutologin)
			open_opt |= OP_NOAUTO;

		for(i=optind; i<argc; i++) {
#ifdef HAVE_GETTIMEOFDAY
			struct timeval beg, end;
			gettimeofday(&beg, 0);
#endif
			yafc_open(argv[i], open_opt, mech, 0);
#ifdef HAVE_GETTIMEOFDAY
			gettimeofday(&end, 0);
			end.tv_sec -= beg.tv_sec;
			if(gvBeepLongCommand && end.tv_sec > gvLongCommandTime)
				fputc('\007', stderr);
#endif
#ifdef HAVE_LIBREADLINE /* add appropriate 'open' command to history */
			asprintf(&s, "open %s%s",
					 argv[i], test(open_opt, OP_ANON) ? " --anon" : "");
			add_history(s);
			free(s);
#endif
		}
	}

	load_ltaglist(false, false, 0);

	command_loop();
	/* should not return */
	list_free(gvFtpList);
  gvFtpList = NULL;
	return 0;
}

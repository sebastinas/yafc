/*
 * input.c -- string input and readline stuff
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
#include "input.h"
#include "strq.h"
#include "completion.h"
#include "gvars.h"
#include "bashline.h"
#include "commands.h"

#ifdef HAVE_HCRYPTO_UI_H
#include <hcrypto/ui.h>
#elif defined(HAVE_OPENSSL_UI_H)
#include <openssl/ui.h>
#elif defined(HAVE_OPENSSL_UI_COMPAT_H)
#include <openssl/ui_compat.h>
#else /* des_read_pw_string and UI_UTIL_read_pw_string are not available */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#if !defined(HAVE_DECL_IUCLC) || !HAVE_DECL_IUCLC
#define IUCLC 0
#endif
#if (!defined(HAVE_DECL_TCGETS) || !HAVE_DECL_TCGETS) && HAVE_DECL_TIOCGETA
#define TCGETS TIOCGETA
#endif
#if (!defined(HAVE_DECL_TCSETS) || !HAVE_DECL_TCSETS) && HAVE_DECL_TIOCSETA
#define TCSETS TIOCSETA
#endif
#endif

#undef __  /* gettext no-op */
#define __(text) (text)

bool readline_running = false;

char *input_read_string(const char *prompt)
{
#ifdef HAVE_LIBREADLINE
	char *ret;
	readline_running = true;
	ret = readline((char *)prompt);
	readline_running = false;
	force_completion_type = cpUnset;
	return ret;
#else
	char tmp[257];
	size_t l;

	fflush(stdin);
	fprintf(stderr, "%s", prompt);
	if(fgets(tmp, 256, stdin) == 0)
		return 0;
	if(tmp[0] == '\n')
		/* return an empty string, "" */
		return xstrdup("");
	tmp[256] = 0;
	l = strlen(tmp);
	/* strip carriage return */
	if(tmp[l-1] == '\n')
		tmp[l-1] = 0;
	return xstrdup(tmp);
#endif
}

/* This is contributed, untested code from an anonymous sender from
 * sourceforge. It didn't compile right away so I commented it out. This is
 * probably a good idea, but I just don't have the time.
 */

/* this compiles ok now, fixes ctrl+c and doesn't use obsolete getpass() */

char *getpass_hook(const char *prompt)
{
#if HAVE_DECL_UI_UTIL_READ_PW_STRING || HAVE_DECLDES_READ_PW_STRING
	char tmp[80];
#if HAVE_DECL_UI_UTIL_READ_PW_STRING
  UI_UTIL_read_pw_string(tmp, sizeof(tmp), (char*)prompt, 0);
#else
	des_read_pw_string(tmp, sizeof(tmp), (char *)prompt, 0);
#endif
	tmp[79] = 0;
	return xstrdup(tmp);
#else
	int c, n = 0;
	char tmp[1024];
	struct termios tbuf, tbufsave;
	FILE *fd;

	if((fd = fopen("/dev/tty", "rb")) == NULL) {
		perror("fopen /dev/tty");
		return NULL;
	}
	if (ioctl(fileno(fd), TCGETS, &tbuf) < 0) {
		perror("ioctl get");
		fclose(fd);
		return NULL;
	}
	tbufsave = tbuf;
	tbuf.c_iflag &= ~(IUCLC | ISTRIP | IXON | IXOFF);
	tbuf.c_lflag &= ~(ICANON | ISIG | ECHO);
	tbuf.c_cc[4] = 1; /* MIN */
	if (ioctl(fileno(fd), TCSETS, &tbuf) < 0) {
		perror("ioctl set");
		fclose(fd);
		return NULL;
	}
	fprintf(stderr, "%s", prompt);
	while ((c = getc(fd)) != '\n') {
		switch (c) {
			case '\b':
				if (n > 0) {
					--n;
					fprintf(stderr, "\b \b");
				} else {
					fprintf(stderr, "\a");
				}
				break;
			default:
				if (n < sizeof(tmp)-1) {
					tmp[n++] = c;
					fprintf(stderr, "*");
				} else {
					fprintf(stderr, "\a");
				}
		}
		fflush(stderr);
	}
	tmp[n] = '\0';
	if (ioctl(fileno(fd), TCSETS, &tbufsave) < 0) {
		perror("ioctl restore");
		fclose(fd);
		return NULL;
	}
	if (fclose(fd) < 0) {
		perror("fclose /dev/tty");
		return NULL;
	}
	fprintf(stderr, "\n");
	return xstrdup(tmp);
#endif
}

char *getuser_hook(const char *prompt)
{
	force_completion_type = cpNone;
	return input_read_string(prompt);
}

int input_read_args(args_t **args, const char *prompt)
{
	char *e, *s;

	args_destroy(*args);
	*args = args_create();

#ifdef HAVE_LIBREADLINE
	rl_completion_append_character = ' ';
#endif
	e = input_read_string(prompt);

	if(!e) {  /* bare EOF received */
		fputc('\n', stderr);
		return EOF;
	}
	s = strip_blanks(e);

	if(!*s) {  /* blank line */
		free(e);
		return 0;
	}


#ifdef HAVE_LIBREADLINE
	add_history(s);
#endif
	args_push_back(*args, s);
	free(e);
	return (*args)->argc;
}

#ifdef HAVE_LIBREADLINE

void input_save_history(void)
{
	if(gvUseHistory) {
		stifle_history(gvHistoryMax);
		write_history(gvHistoryFile);
	}
}

char *remote_completion_function(char *text, int state);

void input_init(void)
{
	rl_outstream = stderr;
    /* Allow conditional parsing of the ~/.inputrc file. */
    rl_readline_name = PACKAGE;
    rl_completion_entry_function = (rl_compentry_func_t *)no_completion_function;
    /* Tell the completer that we want a crack first. */
    rl_attempted_completion_function = (CPPFunction *)the_complete_function;

	rl_completer_word_break_characters = " \t\n\"\';";
	rl_completer_quote_characters = "'\"\\";
	/* characters that need to be quoted when appearing in filenames. */
	rl_filename_quote_characters = " \t\n\\\"'@<>=;|&()#$`?*[]!:";
	rl_filename_quoting_function = bash_quote_filename;
	rl_filename_dequoting_function = (rl_dequote_func_t *)bash_dequote_filename;
	rl_char_is_quoted_p = char_is_quoted;

	force_completion_type = cpUnset;

	if(gvUseHistory)
		read_history(gvHistoryFile);
}

void input_redisplay_prompt(void)
{
	rl_forced_update_display();
}

#else /* HAVE_READLINE */

void input_init(void)
{
}

void input_save_history(void)
{
}

void input_redisplay_prompt(void)
{
}

#endif /* HAVE_LIBREADLINE */

struct foo {
	int opt;
	char *str;
};
static const struct foo question[] = {
	{ASKYES, __("&yes")},
	{ASKNO, __("&no")},
	{ASKCANCEL, __("&cancel")},
	{ASKALL, __("&all")},
	{ASKUNIQUE, __("&unique")},
	{ASKRESUME, __("&resume")},
	{0, 0}
};

static char ask_shortcut(const struct foo *bar)
{
	char *e = strchr(bar->str, '&');
	if(!e || !e[0] || !e[1])
		return 0;
	return tolower((int)e[1]);
}

static char *choice_str(int opt, int def)
{
	static char tmp[32];
	int i, j;

	for(j=i=0; question[i].str; i++) {
		if(test(opt, 1<<i)) {
			char sc = ask_shortcut(&question[i]);
			if(sc == 0)
				return "internal error!";

			tmp[j++] = ((def == (1 << i)) ? toupper((int)sc) : sc);
		}
	}
	tmp[j] = '\0';
	strcat(tmp, ", ? for help");
	return tmp;
}

static void ask_print_help(int opt, int def)
{
	int i;

	for(i=0; question[i].str; i++) {
		if(test(opt, question[i].opt)) {
			char *e = question[i].str;
			fprintf(stderr, "  ");
			while(*e) {
				if(*e == '&') {
					e++;
					fprintf(stderr, "[%c]",
							def == question[i].opt ? toupper((int)*e) : *e);
				} else
					fprintf(stderr, "%c", *e);
				e++;
			}
		}
	}
	fprintf(stderr, "\n");
}

int ask(int opt, int def, const char *prompt, ...)
{
	va_list ap;
	char *chstr;
	char ch;
	int i;
	char *defstr = 0;
	char *e;
	char tmp[81];

	va_start(ap, prompt);
	vasprintf(&e, prompt, ap);
	va_end(ap);

	for(i=0; question[i].str; i++) {
		if(def == question[i].opt) {
			defstr = question[i].str;
			break;
		}
	}

	chstr = choice_str(opt, def);
	while(true) {
		if(gvSighupReceived)
			return def;

		fprintf(stderr, "%s [%s] ", e, chstr);
		if(fgets(tmp, 80, stdin) == 0) {
#if 0
			fputc('\n', stderr);
			continue; /* skip EOF chars */
#else
			return def;
#endif
		}

		if(tmp[0] == '\n')
			return def;

		ch = tolower((int)tmp[0]);

		if(ch == '?') {
			ask_print_help(opt, def);
			continue;
		}

		for(i=0; question[i].str; i++) {
			if(ch == ask_shortcut(&question[i])
				&& test(opt, question[i].opt))
		    return question[i].opt;
		}
	}
	return def;  /* should never happen */
}

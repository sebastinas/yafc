/* utils.c -- small (generic) functions
 * 
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998-2001 martin HedenfaLk
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "syshdr.h"
#include "ftp.h"
#include "linklist.h"
#include "strq.h"
#include "gvars.h"
#include "prompt.h"

void listify_string(const char *str, list *lp)
{
	char *e;
	char *s, *orgs;

	orgs = s = xstrdup(str);
	while((e = strqsep(&s, ':')) != 0) {
		if(list_search(lp, (listsearchfunc)strcmp, e) == 0)
			list_additem(lp, xstrdup(e));
	}
	xfree(orgs);
}

char *stringify_list(list *lp)
{
	listitem *li;
	char *str;

	if(!lp)
		return 0;

	li = lp->first;
	if(li)
		str = xstrdup((char *)li->data);
	else
		return 0;

	for(li = li->next; li; li = li->next) {
		asprintf(&str, "%s:%s",  str, (char *)li->data);
	}
	return str;
}

char *make_unique_filename(const char *path)
{
	char *f = (char *)xmalloc(strlen(path)+10);
	char *ext;
	unsigned n = 0;

	strcpy(f, path);
	ext = f + strlen(f);

	while(1) {
		if(access(f, F_OK) != 0)
			break;
		sprintf(ext, ".%u", ++n);
	}
	return f;
}

char *human_size(long size)
{
	static char buf[17];

	if(size < 1024)
		sprintf(buf, "%lu", size);
	/* else if(size < 1024*1024) */
	else if(size < 999.5*1024)
		sprintf(buf, "%.1fk", (double)size/1024);
	else if(size < 999.5*1024*1024)
		sprintf(buf, "%.2fM", (double)size/(1024*1024));
	else
		sprintf(buf, "%.2fG", (double)size/(1024*1024*1024));
	/* they aren't transferring TB with ftp, eh? */
	
	return buf;
}

char *human_time(unsigned int secs)
{
	static char buf[17];

	if(secs < 60*60)
		sprintf(buf, "%u:%02u", secs/60, secs%60);
	else
		sprintf(buf, "%u:%02u:%02u", secs/(60*60), (secs/60)%60, secs%60);
	return buf;
}

static void print_xterm_title_string(const char *str)
{
	if(gvXtermTitleTerms && strstr(gvXtermTitleTerms, gvTerm) != 0 && str)
		fprintf(stderr, "%s", str);
}

void print_xterm_title(void)
{
	char *xterm_title = expand_prompt(ftp_connected()
									  ? (ftp_loggedin() ? gvXtermTitle3
										 : gvXtermTitle2)
									  : gvXtermTitle1);
	print_xterm_title_string(xterm_title);
	xfree(xterm_title);
}

void reset_xterm_title(void)
{
	char *e;

	asprintf(&e, "\x1B]0;%s\x07", gvTerm);
	print_xterm_title_string(e);
	xfree(e);
}

char* get_mode_string(mode_t m)
{
	static char tmp[4];

	strcpy(tmp, "000");

	if(test(m, S_IRUSR))
		tmp[0] += 4;
	if(test(m, S_IWUSR))
		tmp[0] += 2;
	if(test(m, S_IXUSR))
		tmp[0]++;

	if(test(m, S_IRGRP))
		tmp[1] += 4;
	if(test(m, S_IWGRP))
		tmp[1] += 2;
	if(test(m, S_IXGRP))
		tmp[1]++;

	if(test(m, S_IROTH))
		tmp[2] += 4;
	if(test(m, S_IWOTH))
		tmp[2] += 2;
	if(test(m, S_IXOTH))
		tmp[2]++;

	return tmp;
}

static int switch_search(const Ftp *f, const char *name)
{
	if(f->url->alias) {
		if(strcmp(f->url->alias, name) == 0)
			return 0;
	}
	return strcmp(f->url->hostname, name);
}

listitem *ftplist_search(const char *str)
{
	if(isdigit((int)str[0]) && strchr(str, '.') == 0) {
		listitem *li;
		int i = 1;
		int n = atoi(str);
		if(n <= 0 || n > list_numitem(gvFtpList)) {
			ftp_err(_("invalid connection number: '%d'\n"), n);
			return 0;
		}
		for(li=gvFtpList->first; li; li=li->next, i++) {
			if(i == n)
				return li;
		}
	} else {
		listitem *li = list_search(gvFtpList,
								   (listsearchfunc)switch_search, str);
		if(li)
			return li;
		else
			ftp_err(_("no such connection open: '%s'\n"), str);
	}
	return 0;
}

char *get_local_curdir(void)
{
	static char buf[PATH_MAX+1];
	getcwd(buf, PATH_MAX);
	return buf;
}

void invoke_shell(char *cmdline)
{
	char *shell;
	pid_t pid;

	ftp_set_signal(SIGINT, SIG_IGN);
	shell = getenv("SHELL");
	if(!shell)
		shell = STD_SHELL;
	pid = fork();
	if(pid == 0) { /* child thread */
		if(cmdline)
			execl(shell, shell, "-c", cmdline, 0);
		else {
			printf(_("Executing '%s', use 'exit' to exit from shell...\n"), shell);
			execl(shell, shell, 0);
		}
		perror(shell);
		exit(1);
	}
	if(pid == -1) {
		perror("fork()");
		return;
	}
	waitpid(pid, 0, 0);  /* wait for child to finish execution */
}

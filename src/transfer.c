/* transfer.c -- 
 * 
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998, 1999, 2000 Martin Hedenfalk
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
#include "shortpath.h"
#include "strq.h"
#include "gvars.h"
#include "args.h"
#include "transfer.h"

/* in get.c */
char *make_unique_filename(const char *path);

static FILE *logfp = 0;
static FILE *mailfp = 0;
static char *nohup_logfile = 0;
static char *nohup_command = 0;
static time_t nohup_start_time;
static time_t nohup_end_time;

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

static int max_printf(int max, const char *fmt, ...)
{
	char *e;
	va_list ap;
	size_t len;
	int ret = 0;

	if(!fmt)
		return 0;

	va_start(ap, fmt);
	vasprintf(&e, fmt, ap);
	va_end(ap);

	if(!e)
		return 0;

	len = strlen(e);

	if(max < 0) {
		/* left justified */
		max = -max;
		if(len >= max)
			ret += printf("%s", e + (len-max));
		else {
			int i;
			ret += printf("%s", e);
			for(i=len; i<max; i++, ret++)
				putchar(' ');
		}
	} else if(max > 0) {
		/* right justified */
		if(len >= max)
			ret += printf("%s", e + (len-max));
		else {
			int i;
			for(i=len; i<max; i++, ret++)
				putchar(' ');
			ret += printf("%s", e);
		}
	} else
		ret += printf("%s", e);

	xfree(e);
	return ret;
}

/* transfer status line codes
 * 
 * %r   - remote filename
 * %R   - remote filename w/path
 * %l   - local filename
 * %L   - local filename w/path
 * %s   - size transferred so far
 * %S   - total size (if available)
 * %e   - ETA (time left)
 * %p   - percent transferred
 * %%   - percent sign
 * %b   - transfer rate (Bps)
 * %B   - transfer rate (Bps) or "stalled" if stalled
 * %t   - time elapsed
 * %T   - time transfer has been stalled
 * %v   - visual progress bar
 */

void transfer(transfer_info *ti)
{
	static int lastlen = 0;
	struct timeval now;
	unsigned int secs, eta;
	float bps;
	const char *e;
	bool leftjust;
	int minlen;
	int number_of_dots;  /* used by visual progress bar (%v) */
	int i;
	int inescape = 0;

	if(gvSighupReceived)
		return;

	while(lastlen--)
		fprintf(stderr, " ");
	fprintf(stderr, "\r");

	lastlen = 0;

	if(!ti)
		return;

	if(ti->finished) {
		if(ti->interrupted)
			printf(_("transfer interrupted\n"));
		else if(ti->ioerror)
			printf(_("transfer I/O error\n"));
	}

	if(ti->size > ti->total_size)
		ti->total_size = ti->size;

	gettimeofday(&now, 0);
	secs = now.tv_sec - ti->start_time.tv_sec;
	bps = (ti->size - ti->restart_size) / (secs ? secs : 1);
	if(ti->total_size != -1L) {
		eta = (ti->total_size - ti->size) / (bps ? bps : 1);
		if(eta == 0)
			eta += !ftp->ti.finished;
	}
	else
		eta = (unsigned) -1;

	e = (ti->finished ? gvTransferEndString
		 : (ti->begin ? gvTransferBeginString
			: gvTransferString));
	while(e && *e) {
		if(*e == '%') {
			leftjust = false;
			minlen = 0;
			e++;
			if(!*e)
				break;
			if(*e == '-') {
				leftjust = true;
				e++;
			}
			if(isdigit((int)*e)) {
				minlen = atoi(e);
				while(isdigit((int)*e))
					e++;
			}
			if(leftjust)
				minlen = -minlen;

			switch(*e) {
			  case 'r':
				lastlen += max_printf(minlen, "%s",
						   shortpath(base_name_ptr(ti->remote_name),
									 leftjust ? -minlen : minlen,
									 ftp->homedir));
				break;
			  case 'R':
				lastlen += max_printf(minlen, "%s",
									  shortpath(ti->remote_name,
												leftjust ? -minlen : minlen,
												ftp->homedir));
				break;
			  case 'l':
				lastlen += max_printf(minlen, "%s",
									  shortpath(base_name_ptr(ti->local_name),
												leftjust ? -minlen : minlen,
												gvLocalHomeDir));
				break;
			  case 'L':
				lastlen += max_printf(minlen, "%s",
									  shortpath(ti->local_name,
												leftjust ? -minlen : minlen,
												gvLocalHomeDir));
				break;
			  case 's':
				lastlen += max_printf(minlen, "%sb", human_size(ti->size));
				break;
			  case 'S':
				lastlen += max_printf(minlen, "%sb",
									  (ti->total_size == -1L
									   ? "??"
									   : human_size(ti->total_size)));
				break;
			  case 'b':
				lastlen += max_printf(minlen < 2 ? minlen : minlen-2,
									  "%sb/s", human_size(bps));
				break;
			  case 'B':
				if(ti->stalled >= 5)
					lastlen += max_printf(minlen, "%s", _("stalled"));
				else
					lastlen += max_printf(minlen < 2 ? minlen : minlen-2,
										  "%sb/s", human_size(bps));
				break;
			  case 'e':
				if(eta != (unsigned) -1)
					lastlen += max_printf(minlen, "%s", human_time(eta));
				else
					lastlen += max_printf(minlen, "%s", "--:--");
				break;
			  case 't':
				lastlen += max_printf(minlen, "%s", human_time(secs));
				break;
			  case '%':
				lastlen += printf("%%");
				break;
			  case 'p':
				if(ti->total_size != -1L)
					lastlen += max_printf(minlen, "%.1f",
										  (double)100*ti->size / (ti->total_size + (ti->total_size ? 0 : 1)));
				else
					lastlen += printf("?");
				break;
			  case 'v':
				if(ti->total_size != -1L) {
					if(ti->total_size == ti->size)
						number_of_dots = minlen;
					else
						number_of_dots = (double)minlen * ti->size / (ti->total_size + 1);
					if(number_of_dots > minlen || number_of_dots < 0)
						/* just in case */
						number_of_dots = minlen;
					i = minlen - number_of_dots;
					while(number_of_dots--)
						lastlen += printf("#");
					while(i--)
						lastlen += printf(" ");
				} else {
					number_of_dots = minlen / 2;
					i = minlen - number_of_dots;
					while(number_of_dots--)
						lastlen += printf(" ");
					if(i) {
						i--;
						lastlen += printf("?");
						while(i--)
							lastlen += printf(" ");
					}
				}
				break;
			  case '{':
				inescape++;
				break;
			  case '}':
				inescape--;
				break;		
			  default:
				lastlen += printf("%%%c", *e);
				break;
			}
		} else {
			putchar(*e);
			if (inescape <= 0) lastlen++;
		}
		e++;
	}

	fputc('\r', stdout);
	fflush(stdout);
}

int transfer_init_nohup(const char *str)
{
	if(!str)
		asprintf(&nohup_logfile, "%s/nohup/nohup.%u",
				 gvWorkingDirectory, getpid());
	else
		nohup_logfile = tilde_expand_home(str, gvLocalHomeDir);

	if(logfp)
		fclose(logfp);
	
	logfp = fopen(nohup_logfile, "w");
	if(!logfp) {
		perror(nohup_logfile);
		xfree(nohup_logfile);
		logfp = 0;
		return -1;
	}

	ftp_err(_("Redirecting output to %s\n"), nohup_logfile);

	setbuf(logfp, 0); /* change buffering */
	return 0;
}

static RETSIGTYPE term_handler(int signum)
{
	time_t now = time(0);

	fprintf(stderr, "%s [%sb of ", ftp->ti.remote_name, human_size(ftp->ti.size));
	fprintf(stderr, "%sb]\n", human_size(ftp->ti.total_size));
	printf(_("SIGTERM (terminate) received, exiting...\n"));
	printf(_("Transfer aborted %s"), ctime(&now));
	if(ftp->ti.remote_name)
		printf(_("%s may not have transferred correctly\n"), ftp->ti.remote_name);

	transfer_mail_msg(_("SIGTERM (terminate) received, exiting...\n"));
	transfer_mail_msg(_("Transfer aborted %s"), ctime(&now));
	transfer_mail_msg(_("%s may not have transferred correctly\n"), ftp->ti.remote_name);

	transfer_end_nohup();
}

void transfer_begin_nohup(int argc, char **argv)
{
	nohup_start_time = time(0);

	ftp_set_signal(SIGHUP, SIG_IGN); /* ignore signals */
	ftp_set_signal(SIGINT, SIG_IGN);
	ftp_set_signal(SIGQUIT, SIG_IGN);
	ftp_set_signal(SIGTSTP, SIG_IGN);
	ftp_set_signal(SIGTERM, term_handler);
	dup2(fileno(logfp), fileno(stdout));
	dup2(fileno(logfp), fileno(stderr));
#if defined(HAVE_SETPROCTITLE) || defined(linux)
	if(gvUseEnvString)
		setproctitle("%s, nohup, %s", ftp->url->hostname, nohup_logfile);
#endif

	printf(_("Connected to %s as user %s\n"),
		   ftp->url->hostname,
		   ftp->url->username);
	if(argv)
		printf(_("Transfer started %s\n"), ctime(&nohup_start_time));
	else
		printf(_("Transfer received SIGHUP %s\n"), ctime(&nohup_start_time));
	nohup_command = argv ? args_cat(argc, argv, 0) : 0;
	if(argv)
		printf(_("Command: '%s'\n"), nohup_command);
	printf("pid: %u\n\n", getpid());
}

void transfer_mail_msg(const char *fmt, ...)
{
	static bool opened = false;
	va_list ap;

	if(!gvNohupMailAddress)
		return;

	if(!opened) {
		char *e;
		char *tmpfilename;

		asprintf(&e, "%s/yafcmail.tmp", gvWorkingDirectory);
		tmpfilename = make_unique_filename(e);
		xfree(e);

		mailfp = fopen(tmpfilename, "w+");
		opened = true;
		if(mailfp) {
			unlink(tmpfilename);
			setbuf(logfp, 0); /* change buffering */
			transfer_mail_msg(_("From: yafc@%s\n"
								"Subject: yafc transfer finished on %s\n"
								"\n"
								"This is an automatic message sent by yafc\n"
								"Your transfer is finished!\n"
								"\n"
								"connected to %s as user %s\n"
								"command was: %s\n"
								"started %s\n"
								"\n"
								"(please do not reply to this mail)\n"
								"\n"),
							  gvLocalHost, gvLocalHost,
							  ftp->url->hostname, ftp->url->username,
							  nohup_command ? nohup_command : "(unknown, SIGHUPed)",
							  ctime(&nohup_start_time));
		}
	}

	if(mailfp == 0)
		return;

	va_start(ap, fmt);
	vfprintf(mailfp, fmt, ap);
	va_end(ap);
}

void transfer_end_nohup(void)
{
	nohup_end_time = time(0);
	printf(_("Done\nTransfer ended %s\n"), ctime(&nohup_end_time));

	if(gvNohupMailAddress) {
		FILE *fp;
		char *cmd;

		asprintf(&cmd, "%s %s", gvSendmailPath, gvNohupMailAddress);
		fp = popen(cmd, "w");
		if(fp == 0)
			printf(_("Unable to send mail (using %s)\n"), gvSendmailPath);
		else {
			transfer_mail_msg("\n");
			rewind(mailfp);
			while(true) {
				int c = fgetc(mailfp);
				if(c == EOF) {
					fclose(mailfp);
					break;
				}
				fputc(c, fp);
			}
			pclose(fp);
		}
	}

	ftp_quit_all();
	list_free(gvFtpList);
	xfree(nohup_logfile);
	xfree(nohup_command);
	gvars_destroy();
	exit(0);
}

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

	for(li=li->next; li; li=li->next) {
		asprintf(&str, "%s:%s",  str, (char *)li->data);
	}
	return str;
}

bool ascii_transfer(const char *mask)
{
	listitem *li;

	li = gvAsciiMasks->first;
	while(li) {
		if(fnmatch((char *)li->data, mask, 0) != FNM_NOMATCH)
			return true;
		li = li->next;
	}
	return false;
}

void transfer_nextfile(list *gl, listitem **li, bool removeitem)
{
	if(removeitem) {
		listitem *tmp = (*li)->next;
		list_removeitem(gl, *li);
		*li = tmp;
	} else
		*li = (*li)->next;
}

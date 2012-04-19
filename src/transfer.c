/*
 * transfer.c --
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
#include "shortpath.h"
#include "strq.h"
#include "gvars.h"
#include "args.h"
#include "transfer.h"
#include "utils.h"

static FILE *logfp = 0;
static FILE *mailfp = 0;
static char *nohup_logfile = 0;
static char *nohup_command = 0;
static time_t nohup_start_time;
static time_t nohup_end_time;

static int max_printf(FILE *fp, int max, const char *fmt, ...)
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
			ret += fprintf(fp, "%s", e + (len-max));
		else {
			int i;
			ret += fprintf(fp, "%s", e);
			for(i=len; i<max; i++, ret++)
				fputc(' ', fp);
		}
	} else if(max > 0) {
		/* right justified */
		if(len >= max)
			ret += fprintf(fp, "%s", e + (len-max));
		else {
			int i;
			for(i=len; i<max; i++, ret++)
				fputc(' ', fp);
			ret += fprintf(fp, "%s", e);
		}
	} else
		ret += fprintf(fp, "%s", e);

	free(e);
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

static int print_transfer_string(char *str,
								 FILE *fp,
								 transfer_info *ti,
								 float bps,
								 unsigned int secs,
								 unsigned long long int eta)
{
	int i;
	int len = 0;
	char *e = str;
	bool leftjust;
	int inescape = 0;
	int minlen;
	int number_of_dots;  /* used by visual progress bar (%v) */

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
				len += max_printf(fp, minlen, "%s",
								  shortpath(base_name_ptr(ti->remote_name),
											leftjust ? -minlen : minlen,
											ftp->homedir));
				break;
			case 'R':
				len += max_printf(fp, minlen, "%s",
								  shortpath(ti->remote_name,
											leftjust ? -minlen : minlen,
											ftp->homedir));
				break;
			case 'l':
				len += max_printf(fp, minlen, "%s",
								  shortpath(base_name_ptr(ti->local_name),
											leftjust ? -minlen : minlen,
											gvLocalHomeDir));
				break;
			case 'L':
				len += max_printf(fp, minlen, "%s",
								  shortpath(ti->local_name,
											leftjust ? -minlen : minlen,
											gvLocalHomeDir));
				break;
			case 's':
				len += max_printf(fp, minlen, "%sB", human_size(ti->size));
				break;
			case 'S':
				len += max_printf(fp, minlen, "%sB",
								  (ti->total_size == -1L
								   ? "??"
								   : human_size(ti->total_size)));
				break;
			case 'b':
				len += max_printf(fp, minlen < 2 ? minlen : minlen-2,
								  "%sB/s", human_size(bps));
				break;
			case 'B':
				if(ti->stalled >= 5)
					len += max_printf(fp, minlen, "%s", _("stalled"));
				else
					len += max_printf(fp, minlen < 2 ? minlen : minlen-2,
									  "%sB/s", human_size(bps));
				break;
			case 'e':
				if(eta != (unsigned) -1)
					len += max_printf(fp, minlen, "%s", human_time(eta));
				else
					len += max_printf(fp, minlen, "%s", "--:--");
				break;
			case 't':
				len += max_printf(fp, minlen, "%s", human_time(secs));
				break;
			case '%':
				len += fprintf(fp, "%%");
				break;
			case 'p':
				if(ti->total_size != -1L)
					len += max_printf(fp, minlen, "%.1f",
									  (double)100*ti->size /
									  (ti->total_size +
									   (ti->total_size ? 0 : 1)));
				else
					len += fprintf(fp, "?");
				break;
			case 'v':
				if(ti->total_size != -1L) {
					if(ti->total_size == ti->size)
						number_of_dots = minlen;
					else
						number_of_dots = (double)minlen *
							ti->size / (ti->total_size + 1);
					if(number_of_dots > minlen || number_of_dots < 0)
						/* just in case */
						number_of_dots = minlen;
					i = minlen - number_of_dots;
					while(number_of_dots--)
						len += fprintf(fp, "#");
					while(i--)
						len += fprintf(fp, " ");
				} else {
					number_of_dots = minlen / 2;
					i = minlen - number_of_dots;
					while(number_of_dots--)
						len += fprintf(fp, " ");
					if(i) {
						i--;
						len += fprintf(fp, "?");
						while(i--)
							len += fprintf(fp, " ");
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
				len += fprintf(fp, "%%%c", *e);
				break;
			}
		} else {
			fputc(*e, fp);
			if (inescape <= 0) len++;
		}
		e++;
	}

	return len;
}

void transfer(transfer_info *ti)
{
	static int lastlen = 0;
	struct timeval now;
	unsigned int secs, eta;
	float bps;

	if(gvSighupReceived)
		return;

	while(lastlen--)
		fprintf(stderr, " ");
	fprintf(stderr, "\r");

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

	lastlen = print_transfer_string(ti->finished ? gvTransferEndString
									: (ti->begin ? gvTransferBeginString
									   : gvTransferString),
									stdout,
									ti,
									bps,
									secs,
									eta);

	if(gvTransferXtermString && gvXtermTitleTerms
	   && strstr(gvXtermTitleTerms, gvTerm) != 0)
		{
			print_transfer_string(gvTransferXtermString,
								  stderr,
								  ti,
								  bps,
								  secs,
								  eta);
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
		free(nohup_logfile);
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

	fprintf(stderr, "%s [%sB of ", ftp->ti.remote_name,
			human_size(ftp->ti.size));
	fprintf(stderr, "%sB]\n", human_size(ftp->ti.total_size));
	printf(_("SIGTERM (terminate) received, exiting...\n"));
	printf(_("Transfer aborted %s"), ctime(&now));
	if(ftp->ti.remote_name)
		printf(_("%s may not have transferred correctly\n"),
			   ftp->ti.remote_name);

	transfer_mail_msg(_("SIGTERM (terminate) received, exiting...\n"));
	transfer_mail_msg(_("Transfer aborted %s"), ctime(&now));
	transfer_mail_msg(_("%s may not have transferred correctly\n"),
					  ftp->ti.remote_name);

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
#if 0 && (defined(HAVE_SETPROCTITLE) || defined(linux))
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
		mailfp = tmpfile();
		opened = true;
		if(mailfp) {
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
							  nohup_command ? nohup_command :
							  "(unknown, SIGHUPed)",
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
	free(nohup_logfile);
	free(nohup_command);
	gvars_destroy();
	exit(0);
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

bool transfer_first(const char *mask)
{
	listitem *li;

	li = gvTransferFirstMasks->first;
	while(li) {
		if(fnmatch((char *)li->data, mask, 0) != FNM_NOMATCH)
			return true;
		li = li->next;
	}
	return false;
}

bool ignore(const char *mask)
{
	listitem *li;

	li = gvIgnoreMasks->first;
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

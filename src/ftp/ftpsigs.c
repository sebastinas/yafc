/* $Id: ftpsigs.c,v 1.5 2001/05/28 09:53:44 mhe Exp $
 *
 * ftpsigs.c -- handles signals
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
#include "ftpsigs.h"
#include "gvars.h"
#include "transfer.h"

/* in cmd.c */
void exit_yafc(void);

void ftp_set_signal_with_mask(int signum, sighandler_t handler, int onesig)
{
	int saved_errno = errno;
#ifdef HAVE_POSIX_SIGNALS
	struct sigaction sa;
	sigset_t ss;

	/* unblock this signal */
	sigemptyset(&ss);
	sigaddset(&ss, signum);
	sigprocmask(SIG_UNBLOCK, &ss, 0);

	/* set the signal handler */
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	if(onesig)
		sigaddset(&sa.sa_mask, onesig); /* a signal to exclude */
	sa.sa_flags = 0;
#ifdef SA_RESTART
	sa.sa_flags |= SA_RESTART;
#endif

# if 0 /* this causes segfaults!? */
	if(signum == SIGALRM) {
#  ifdef SA_INTERRUPT
		sa.sa_flags |= SA_INTERRUPT; /* old, obsolete flag? */
#  endif
	} else {
#  ifdef SA_RESTART
		sa.sa_flags |= SA_RESTART;
#  endif
	}
# endif

	/*	sa.sa_restorer = 0;*/

	if(sigaction(signum, &sa, 0) != 0)
		ftp_trace("couldn't set signalhandler for signal %d\n", signum);
#else
	signal(signum, handler);
#endif
	errno = saved_errno;
}

void ftp_set_signal(int signum, sighandler_t handler)
{
	ftp_set_signal_with_mask(signum, handler, 0);
}

static unsigned int sigints = 0;

RETSIGTYPE sigint_close_handler(int signum)
{
	if(gvSighupReceived)
		return;

	ftp_trace("Interrupt received\n");
	sigints++;

	gvInterrupted = true;

	/* close connection and restart command loop after 4 interrupts
	 *
	 * If we're in SSH mode, the interrupt signal is intercepted by
	 * the ssh program as well, which terminates directly. The ssh
	 * program is started by ssh_connect() in ssh_ftp.c by forking and
	 * execv'ing the program in the child process. I don't know if
	 * there is a way to prevent the interrupt signal to reach the ssh
	 * program?
	 */

	if(sigints >= 4 || ftp->ssh_pid) {
		ftp_close();
		sigints = 0;
		if(gvJmpBufSet) {
			ftp_trace("jumping to gvRestartJmp\n");
			ftp_longjmp(gvRestartJmp, 1);
		} else {
			exit(17);
		}
	}
	else if(sigints == 3) {
		if(gvJmpBufSet)
			ftp_err(_("OK, one more to close connection...          \n"));
		else
			ftp_err(_("OK, one more to exit program...          \n"));
	}

	/* disable any alarm set */
	alarm(0);
	ftp_set_signal(SIGALRM, SIG_DFL);

	/* re-install the signal handler */
/*	ftp_set_signal(SIGINT, sigint_close_handler);*/
}

RETSIGTYPE sigint_abort_handler(int signum)
{
	if(gvSighupReceived)
		return;

	ftp_trace("Interrupt received\n");
	sigints++;

	gvInterrupted = true;

	if(sigints >= 3 || ftp->ssh_pid) {
		sigints = 0;
		alarm(0);
		if(gvJmpBufSet) {
			ftp_trace("jumping to gvRestartJmp\n");
			ftp_flush_reply();
			ftp_longjmp(gvRestartJmp, 1);
		} else
			exit(17);
	} else if(sigints == 2)
		ftp_err(_("OK, one more to abort command...          \n"));

	/* re-install the signal handler */
/*	ftp_set_signal(SIGINT, sigint_abort_handler);*/
}

RETSIGTYPE sigint_jmp_handler(int signum)
{
	if(gvSighupReceived)
		return;

	ftp_trace("Interrupt received         \n");
	alarm(0);
	if(gvJmpBufSet) {
		ftp_trace("jumping to gvRestartJmp\n");
		if(ftp->ssh_pid)
			ftp_close();
		ftp_longjmp(gvRestartJmp, 1);
	} else {
		ftp_err(_("Interrupt received, exiting...         \n"));
		exit(17);
	}
}

#if 0
static RETSIGTYPE sighup_term_handler(int signum)
{
	printf(_("SIGTERM (terminate) received, exiting...\n"));
	ftp_close();
	ftp_destroy(gvFtp);
	gvars_destroy();
	exit(19);
}
#endif

RETSIGTYPE sighup_handler(int signum)
{
	ftp_set_signal(SIGINT, SIG_IGN);
	ftp_set_signal(SIGHUP, sighup_handler);
	if(gvSighupReceived)
		return;

	gvSighupReceived = true;
	if(gvInTransfer)
		ftp_err(_("Hangup received, continuing transfer in background...\n"));
	else {
		ftp_err(_("Hangup received, exiting...\n"));
		exit_yafc();
	}

	if(transfer_init_nohup(0) != 0) {
		if(transfer_init_nohup("/dev/null") != 0) {
			ftp_err(_("Can't redirect output, exiting...\n"));
			ftp_quit();
			exit(18);
		}
	}

	transfer_begin_nohup(0, 0);
	sigints = 0;
}

int ftp_longjmp(
#ifdef HAVE_POSIX_SIGSETJMP
				sigjmp_buf restart_jmp
#else
				jmp_buf restart_jmp
#endif
				, int arg)
{
#ifdef HAVE_POSIX_SIGSETJMP
	siglongjmp(restart_jmp, 1);
#else
	longjmp(restart_jmp, 1);
#endif
	return 0; /* make cc happy */
}

void ftp_initsigs(void)
{
	sigints = 0;
	ftp_set_signal(SIGPIPE, SIG_IGN);
	ftp_set_signal(SIGINT, gvSighupReceived ? SIG_IGN : sigint_jmp_handler);
	ftp_set_signal_with_mask(SIGHUP, sighup_handler, SIGINT);
}

void ftp_sigint_close_reset(void)
{
	sigints = 0;
	ftp_initsigs();
}

void ftp_set_close_handler(void)
{
	sigints = 0;
	ftp_set_signal(SIGINT, gvSighupReceived ? SIG_IGN : sigint_close_handler);
}

void ftp_set_abort_handler(void)
{
	sigints = 0;
	ftp_set_signal(SIGINT, gvSighupReceived ? SIG_IGN : sigint_abort_handler);
}

unsigned int ftp_sigints(void)
{
	return sigints;
}

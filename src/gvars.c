/* gvars.c -- global variables
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
#include "linklist.h"

/* list of Ftp objects */
list *gvFtpList = 0;

/* pointer to an Ftp item in gvFtpList */
listitem *gvCurrentFtp = 0;

FILE *gvLogfp = 0;

char *gvWorkingDirectory = 0;

/* try to login automagically */
bool gvAutologin = true;

/* issue a SYST command upon login */
bool gvStartupSyst = false;

/* use tab completion for remote files */
bool gvRemoteCompletion = true;

/* quit program when Ctrl-D is pressed */
bool gvQuitOnEOF = true;

char *gvXtermTitle1 = 0;
char *gvXtermTitle2 = 0;
char *gvXtermTitle3 = 0;
char *gvXtermTitleTerms = 0;
char *gvTerm = 0;

/* password used for anonymous logins */
char *gvAnonPasswd = 0;

/* name of local host */
char *gvLocalHost = 0;

/* not connected */
char *gvPrompt1;
/* connected but not logged in */
char *gvPrompt2;
/* logged in */
char *gvPrompt3;

/* beep if didn't complete within gvLongCommandTime seconds */
bool gvBeepLongCommand = true;
/* number of seconds for command to be 'long' */
int gvLongCommandTime = 30;

bool gvSighupReceived = false;
bool gvInTransfer = false;
bool gvInterrupted = false;

/* list of visited hosts */
list *gvUrlHistory = 0;  /* list of url_t's */
/* bookmark list */
list *gvBookmarks = 0;      /* list of url_t's */
/* used for host completion */
list *gvHostCompletion = 0;

url_t *gvDefaultUrl = 0;
url_t *gvLocalUrl = 0;

list *gvLocalTagList = 0; /* list of char* */

/* default transfer type, binary (image) or ascii */
transfer_mode_t gvDefaultType = tmBinary;

/* used by bookmark --edit */
char *gvEditor = 0;

/* name of user running yafc */
char *gvUsername = 0;

/* print what yafc is doing in environment string
 * (seen when issuing a ps)
 */
bool gvUseEnvString = true;

/* list of aliases */
list *gvAliases = 0;

/* list of shell-glob-format filemasks to transfer in ascii mode */
list *gvAsciiMasks = 0;  /* list of (char *) */

bool gvUseHistory = true;
int gvHistoryMax = 128;

char *gvLocalHomeDir = 0;
char *gvLocalPrevDir = 0;
char *gvHistoryFile = 0;

bool gvVerbose = false;
bool gvDebug = false;
bool gvTrace = false;
bool gvPasvmode = false;
bool gvReadNetrc = true;

int gvAutoBookmark = 0; /* 0 = no, 1 = yes, 2 = ask */
bool gvAutoBookmarkSilent = false;
bool gvAutoBookmarkSavePasswd = false;

int gvLoadTaglist = 2;  /* 0 = no, 1 = yes, 2 = ask */

bool gvTilde = true;

/* seconds to wait before attempting to re-connect */
int gvConnectWaitTime = 30;
/* number of times to try to re-connect */
unsigned int gvConnectAttempts = 10;

unsigned int gvCommandTimeout = 42;
unsigned int gvConnectionTimeout = 30;

/* mailaddress to send mail to when nohup transfer is finished */
char *gvNohupMailAddress = 0;
/* path to sendmail */
char *gvSendmailPath = 0;

char *gvTransferBeginString = 0;
char *gvTransferString = 0;
char *gvTransferEndString = 0;

int gvProxyType = 0;
url_t *gvProxyUrl = 0;
list *gvProxyExclude = 0;

#ifdef HAVE_POSIX_SIGSETJMP
sigjmp_buf gvRestartJmp;
#else
jmp_buf gvRestartJmp;
#endif
bool gvJmpBufSet = false;

void gvars_destroy(void)
{
	xfree(gvEditor);
	xfree(gvAnonPasswd);
	xfree(gvPrompt1);
	xfree(gvPrompt2);
	xfree(gvPrompt3);
	xfree(gvLocalHomeDir);
	xfree(gvLocalPrevDir);
	xfree(gvUsername);
	xfree(gvHistoryFile);
	xfree(gvNohupMailAddress);
	xfree(gvSendmailPath);
	xfree(gvLocalHost);
	xfree(gvTransferBeginString);
	xfree(gvTransferString);
	xfree(gvTransferEndString);
	list_free(gvUrlHistory);
	list_free(gvAsciiMasks);
	list_free(gvAliases);
	list_free(gvLocalTagList);
	list_free(gvBookmarks);
	url_destroy(gvDefaultUrl);
	url_destroy(gvLocalUrl);

	if(gvLogfp)
		fclose(gvLogfp);
	gvLogfp = 0;
}

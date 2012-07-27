/*
 * gvars.h -- global variables
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _gvars_h_included
#define _gvars_h_included

#include "syshdr.h"
#include "ftp.h"
#include "linklist.h"
#include "stats.h"

/* time (in seconds) before a cached directory times out, 0 == never */
extern int gvCacheTimeout;

/* list of Ftp objects */
extern list *gvFtpList;

/* pointer to an Ftp item in gvFtpList */
extern listitem *gvCurrentFtp;

extern FILE *gvLogfp;

extern char *gvWorkingDirectory;

/* try to login automagically */
extern bool gvAutologin;

/* do reverse DNS lookups? */
extern bool gvReverseDNS;

/* add ... while waiting for a completion? */
extern bool gvWaitingDots;

/* custom user options like -C */
extern char *gvSSHOptions;

/* try to use scp */
extern bool gvSSHTrySCP;

/* automatically reconnect on connection timeout */
extern bool gvAutoReconnect;

extern bool gvVerbose;
extern bool gvDebug;
extern bool gvTrace;
extern bool gvPasvmode;
extern bool gvReadNetrc;

/* default security mechanism to use */
extern list *gvDefaultMechanism;  /* list of (char *) */

extern char *gvXtermTitle1;
extern char *gvXtermTitle2;
extern char *gvXtermTitle3;
extern char *gvXtermTitleTerms;
extern char *gvTerm;

/* issue a SYST command upon login */
extern bool gvStartupSyst;

/* use tab completion for remote files */
extern bool gvRemoteCompletion;

/* bookmark list */
extern list *gvBookmarks;      /* list of url_t's */

extern bool gvSighupReceived;
extern bool gvInTransfer;
extern bool gvInterrupted;

/* quit program when Ctrl-D is pressed */
extern bool gvQuitOnEOF;

/* display ~ instead of full home directory path */
extern bool gvTilde;

/* password used for anonymous logins */
extern char *gvAnonPasswd;

extern char *gvLocalHost;

extern char *gvUsername;

extern bool gvUseEnvString;

/* not connected */
extern char *gvPrompt1;
/* connected but not logged in */
extern char *gvPrompt2;
/* logged in */
extern char *gvPrompt3;

/* beep if didn't complete within gvLongCommandTime seconds */
extern bool gvBeepLongCommand;
/* number of seconds for command to be 'long' */
extern int gvLongCommandTime;

extern bool gvUseHistory;
extern int gvHistoryMax;

/* default transfer type, image or ascii */
extern transfer_mode_t gvDefaultType;

extern char *gvEditor;

/* list of aliases */
extern list *gvAliases;

/* list of shell-glob-format filemasks to transfer in ascii mode */
extern list *gvAsciiMasks;

/* list of shell-glob-format filemasks to transfer before other files */
extern list *gvTransferFirstMasks;

/* list of shell-glob-format filemasks to never transfer */
extern list *gvIgnoreMasks;

/* don't include "." and ".." in completions */
extern bool gvCompletionSkipDotdirs;

extern int gvAutoBookmark;
extern int gvAutoBookmarkUpdate;
extern bool gvAutoBookmarkSilent;
extern bool gvAutoBookmarkSavePasswd;

extern int gvLoadTaglist;

extern url_t *gvDefaultUrl;
extern url_t *gvLocalUrl;

extern list *gvLocalTagList; /* list of char* */

extern char *gvLocalHomeDir;
extern char *gvLocalPrevDir;
extern char *gvHistoryFile;

extern int gvConnectWaitTime;
extern unsigned int gvConnectAttempts;
extern unsigned int gvCommandTimeout;
extern unsigned int gvConnectionTimeout;
extern char *gvNohupMailAddress;
extern char *gvSendmailPath;

extern char *gvTransferBeginString;
extern char *gvTransferString;
extern char *gvTransferEndString;
extern char *gvTransferXtermString;

extern Stats *gvStatsTransfer;
extern int gvStatsThreshold;

extern int gvProxyType;
extern url_t *gvProxyUrl;
extern list *gvProxyExclude;

#ifdef HAVE_POSIX_SIGSETJMP
extern sigjmp_buf gvRestartJmp;
#else
extern jmp_buf gvRestartJmp;
#endif
extern bool gvJmpBufSet;

void gvars_destroy(void);

#endif

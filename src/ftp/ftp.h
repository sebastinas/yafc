/* $Id: ftp.h,v 1.15 2004/05/20 11:10:52 mhe Exp $
 *
 * ftp.h -- lower level FTP stuff
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _ftp_h_included
#define _ftp_h_included

#include "syshdr.h"

#include "host.h"
#include "socket.h"
#include "url.h"
#include "linklist.h"
#include "rdirectory.h"
#include "rfile.h"
#include "ftpsigs.h"
#include "rglob.h"
#include "args.h"

#define MAXREPLY 512      /* max size of (one line of) reply string */

#define FTP_BUFSIZ 4096

#define ALARM_SEC 0
#define ALARM_USEC 500000

typedef enum {
	putNormal,
	putAppend,
	putUnique,
	putResume
} putmode_t;

typedef enum {
	getNormal,
	getAppend,
	getResume,
	getPipe
} getmode_t;

typedef enum {
	fxpNormal,
	fxpAppend,
	fxpUnique,
	fxpResume
} fxpmode_t;

typedef enum {       /* reply codes */
	ctNone,          /* no reply, (eg connection lost) */
	ctPrelim,        /* OK, gimme' more input */
	ctComplete,      /* OK, command completed */
	ctContinue,      /* OK, send next command in sequence (eg RNFR, RNTO) */
	ctTransient,     /* error, try later */
	ctError          /* unrecoverable error, don't try this again */
} code_t;

typedef enum {    /* verbosity levels */
	vbUnset,      /* undefined, used internally */
	vbNone,       /* never print anything */
	vbError,      /* only print errors (code >= 400) without code */
	vbCommand,    /* print all replies w/o code */
	vbDebug       /* always print everything, including sent commands */
} verbose_t;

typedef enum {
	tmAscii,      /* use ascii mode conversion */
	tmBinary,     /* no conversion, raw binary */
	tmCurrent     /* use current mode (ftp->prev_mode) */
} transfer_mode_t;

typedef enum {
	ltUnknown,
	ltUnix,
	ltDos,
	ltEplf,
	ltMlsd
} LIST_t;

typedef enum {
	mechNone,
	mechKrb4,
	mechKrb5,
	mechSSL
} mech_type_t;

typedef struct transfer_info
{
	char *remote_name;           /* name of remote file or 0 */
	char *local_name;            /* name of local file or 0 */
	long long total_size;             /* total size in bytes or -1 */
	long long size;                   /* size in bytes transferred so far */
	long long restart_size;           /* restart size */
	struct timeval start_time;   /* time of transfer start */
	bool interrupted;            /* true if transfer interrupted (w/SIGINT) */
	bool ioerror;                /* true if I/O error occurred */
	unsigned stalled;            /* incremented when stalled (no I/O) */
	unsigned barelfs;            /* number of bare LF's received (ascii) */
	bool transfer_is_put;        /* true if transfer is put (upload) */
	bool finished;               /* set when transfer finished */
	bool begin;
} transfer_info;

typedef void (*ftp_transfer_func)(transfer_info *ti);

typedef struct Ftp
{
	Socket *ctrl, *data;
	Host *host;
	url_t *url;

	list *cache;             /* list of rdirectory */
	list *dirs_to_flush;     /* list of (char *) */

	pid_t ssh_pid;           /* process id of ssh program, or 0 if
							  * ssh is not in use */
	args_t *ssh_args;
	int ssh_in, ssh_out;     /* file descriptors for use by ssh */
	int ssh_version;
	unsigned int ssh_id;
	int ssh_last_status;

	char reply[MAXREPLY+1];  /* last reply string from server */

	code_t code;  /* last reply code (1-5) */
	int fullcode; /* last reply code (XYZ) */

	/* ftp_read_reply() will timeout in this many seconds
	 * calls ftp_close() and jumps to restart_jmp on timeout
	 */
	unsigned int reply_timeout; /* 0 == no timeout */
	unsigned int open_timeout;

	bool connected;
	bool loggedin;
	verbose_t verbosity, tmp_verbosity;

	bool has_mdtm_command;
	bool has_size_command;
	bool has_pasv_command;
	bool has_stou_command;
	bool has_site_chmod_command;
	bool has_site_idle_command;
	bool has_mlsd_command;

	long restart_offset;  /* next transfer will be restarted at this offset */

	char *homedir;  /* home directory (curdir on startup) */
	char *curdir;   /* current directory */
	char *prevdir;  /* previous directory */

	list *taglist;  /* list of rfile */

	transfer_mode_t prev_type; /* previous data transfer type */

	LIST_t LIST_type;

	/* these functions should return a malloc()'d string or 0
	 * and print the prompt, returned string is free()'d
	 */
	char * (*getuser_hook)(const char *prompt);
	char * (*getpass_hook)(const char *prompt);

#ifdef SECFTP
	bool sec_complete;
	enum protection_level data_prot;
	enum protection_level command_prot;
	size_t buffer_size;
	void *app_data;

	struct buffer in_buffer, out_buffer;
	int request_data_prot;

	struct sec_client_mech *mech;
#endif

	char *last_mkpath; /* used to speed up ftp_mkpath() */

	transfer_info ti;

} Ftp;

extern Ftp *ftp;

void ftp_vtrace(const char *fmt, va_list ap);
void ftp_trace(const char *fmt, ...);
void ftp_err(const char *fmt, ...);

Ftp *ftp_create(void);
void ftp_destroy(Ftp *ftp);
void ftp_use(Ftp *ftp);
void ftp_reset_vars(void);
int ftp_set_trace(const char *filename);

void ftp_reply_timeout(unsigned int secs);
int ftp_cmd(const char *cmd, ...);
int ftp_open(const char *host);
int ftp_reopen(void);
int ftp_open_host(Host *hostp);
int ftp_open_url(url_t *urlp, bool reset_vars);
int ftp_login(const char *guessed_username, const char *anonpass);

void ftp_close();
void ftp_quit();
void ftp_quit_all(void);

int ftp_read_reply();
const char *ftp_getreply(bool withcode);

int ftp_list(const char *cmd, const char *param, FILE *fp);
int ftp_receive(const char *path, FILE *fp,
				transfer_mode_t mode, ftp_transfer_func hookf);
int ftp_getfile(const char *infile, const char *outfile, getmode_t how,
				transfer_mode_t mode, ftp_transfer_func hookf);
int ftp_putfile(const char *infile, const char *outfile, putmode_t how,
				transfer_mode_t mode, ftp_transfer_func hookf);
int ftp_fxpfile(Ftp *srcftp, const char *srcfile,
				Ftp *destftp, const char *destfile,
				fxpmode_t how, transfer_mode_t mode);

int ftp_reset();
void reset_transfer_info(void);
void transfer_finished(void);

int ftp_get_verbosity(void);
void ftp_set_verbosity(int verbosity);
void ftp_set_tmp_verbosity(int verbosity);
int get_username(url_t *url, const char *guessed_username, bool isproxy);

bool ftp_connected(void);
bool ftp_loggedin(void);

rdirectory *ftp_get_directory(const char *path);
rdirectory *ftp_read_directory(const char *path);
rdirectory *ftp_cache_get_directory(const char *path);
rfile *ftp_cache_get_file(const char *path);
rfile *ftp_get_file(const char *path);
void ftp_cache_list_contents(void);
void ftp_cache_flush_mark(const char *p);
void ftp_cache_flush_mark_for(const char *p);
void ftp_cache_flush(void);
void ftp_cache_clear(void);

char *ftp_getcurdir(void);
void ftp_update_curdir_x(const char *p);

int ftp_chdir(const char *path);
int ftp_mkdir(const char *path);
int ftp_mkpath(const char *path);
int ftp_rmdir(const char *path);
int ftp_rename(const char *oldname, const char *newname);
int ftp_cdup(void);
int ftp_unlink(const char *path);
int ftp_chmod(const char *path, const char *mode);
time_t ftp_filetime(const char *filename);
unsigned long long ftp_filesize(const char *path);
int ftp_idle(const char *idletime);
int ftp_noop(void);
int ftp_help(const char *arg);
int ftp_set_protlevel(const char *protlevel);
char *ftp_path_absolute(const char *path);
void ftp_flush_reply(void);
int ftp_type(transfer_mode_t type);
time_t gmt_mktime(const struct tm *ts);

/* returns:
 * 0 == is not a directory
 * 1 == is a real directory
 * 2 == is a link not in cache, maybe dir
 */
int ftp_maybe_isdir(rfile *fp);
void ftp_pwd(void);
char *perm2string(int perm);
void ftp_get_feat(void);

/* in lscolors.h */
void init_colors(void);



void connect_to_server(char **args, int *in, int *out, pid_t *sshpid);
char ** make_ssh_args(char ***args, char *add_arg);

#endif

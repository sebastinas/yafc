/* ftp.h -- lower level FTP stuff
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

#define MAXREPLY 512      /* max size of (one line of) reply string */

#define FTP_BUFSIZ 4096

#define ALARM_SEC 0
#define ALARM_USEC 500000

typedef enum {
	putNormal,
	putAppend,
	putUnique,
	putResume,
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
	ltEplf
} LIST_t;

typedef struct transfer_info
{
	char *remote_name;     /* name of remote file or 0 */
	char *local_name;      /* name of local file or 0 */
	long total_size;             /* total size in bytes or -1 */
	long size;                   /* size in bytes transferred so far */
	long restart_size;           /* restart size */
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
	bool pasvmode;   /* use passive mode data connections */
	verbose_t verbosity, tmp_verbosity;

	bool has_mdtm_command;
	bool has_size_command;
	bool has_pasv_command;
	bool has_stou_command;
	bool has_site_chmod_command;
	bool has_site_idle_command;

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

#if defined(KRB4) || defined(KRB5)
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
int ftp_open_host(Host *hostp);
int ftp_open_url(url_t *urlp);
int ftp_login(const char *guessed_username, const char *anonpass);

void ftp_close();
void ftp_quit();
void ftp_quit_all(void);

int ftp_read_reply();
const char *ftp_getreply(bool withcode);

int ftp_list(const char *cmd, const char *param, FILE *fp);

int ftp_init_receive(const char *path, transfer_mode_t mode,
					 ftp_transfer_func hookf);
int ftp_do_receive(FILE *fp,
				   transfer_mode_t mode, ftp_transfer_func hookf);
int ftp_receive(const char *path, FILE *fp,
				transfer_mode_t mode, ftp_transfer_func hookf);
int ftp_getfile(const char *infile, const char *outfile, getmode_t how,
				transfer_mode_t mode, ftp_transfer_func hookf);

int ftp_send(const char *path, FILE *fp, putmode_t how,
			 transfer_mode_t mode, ftp_transfer_func hookf);
int ftp_putfile(const char *infile, const char *outfile, putmode_t how,
				transfer_mode_t mode, ftp_transfer_func hookf);

int ftp_fxpfile(Ftp *srcftp, const char *srcfile,
				Ftp *destftp, const char *destfile,
				fxpmode_t how, transfer_mode_t mode);

int ftp_reset();

int ftp_get_verbosity(void);
void ftp_set_verbosity(int verbosity);
void ftp_set_tmp_verbosity(int verbosity);

bool ftp_connected(void);
bool ftp_loggedin(void);
void ftp_setpasvmode(void);

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

int ftp_chdir(const char *path);
int ftp_mkdir(const char *path);
int ftp_mkpath(const char *path);
int ftp_rmdir(const char *path);
int ftp_rename(const char *oldname, const char *newname);
int ftp_cdup(void);
int ftp_unlink(const char *path);
int ftp_chmod(const char *path, const char *mode);
time_t ftp_filetime(const char *filename);
unsigned long ftp_filesize(const char *path);
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

/* in lscolors.h */
void init_colors(void);

#endif

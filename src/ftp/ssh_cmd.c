/* Modified for use in Yafc by Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * Original copyright:
 *
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "syshdr.h"
#include "ftp.h"
#include "strq.h"
#include "gvars.h"
/*#include "pathnames.h"*/
#include "rfile.h"

/* from libssh examples */
static int verify_knownhost(ssh_session session)
{
  unsigned char *hash = NULL;
  char *hexa;
  char buf[10];

  int state = ssh_is_server_known(session);
  int hlen = ssh_get_pubkey_hash(session, &hash);
  if (hlen < 0)
    return -1;

  switch (state)
  {
    case SSH_SERVER_KNOWN_OK:
      break; /* ok */

    case SSH_SERVER_KNOWN_CHANGED:
      fprintf(stderr, "Host key for server changed: it is now:\n");
      ssh_print_hexa("Public key hash", hash, hlen);
      fprintf(stderr, "For security reasons, connection will be stopped\n");
      free(hash);
      return -1;

    case SSH_SERVER_FOUND_OTHER:
      fprintf(stderr, "The host key for this server was not found but an other"
        "type of key exists.\n");
      fprintf(stderr, "An attacker might change the default server key to"
        "confuse your client into thinking the key does not exist\n");
      free(hash);
      return -1;

    case SSH_SERVER_FILE_NOT_FOUND:
      fprintf(stderr, "Could not find known host file.\n");
      fprintf(stderr, "If you accept the host key here, the file will be"
       "automatically created.\n");
      /* fallback to SSH_SERVER_NOT_KNOWN behavior */

    case SSH_SERVER_NOT_KNOWN:
      hexa = ssh_get_hexa(hash, hlen);
      fprintf(stderr,"The server is unknown. Do you trust the host key?\n");
      fprintf(stderr, "Public key hash: %s\n", hexa);
      free(hexa);
      if (fgets(buf, sizeof(buf), stdin) == NULL)
      {
        free(hash);
        return -1;
      }
      if (strncasecmp(buf, "yes", 3) != 0)
      {
        free(hash);
        return -1;
      }
      if (ssh_write_knownhost(session) < 0)
      {
        fprintf(stderr, "Error %s\n", strerror(errno));
        free(hash);
        return -1;
      }
      break;

    case SSH_SERVER_ERROR:
      fprintf(stderr, "Error %s", ssh_get_error(session));
      free(hash);
      return -1;
  }

  free(hash);
  return 0;
}

static int authenticate_pubkey(ssh_session session)
{
  return ssh_userauth_publickey_auto(session, NULL);
}

static int authenticate_password(ssh_session session)
{
  char* password = getpass("Enter your password: ");
  return ssh_userauth_password(session, NULL, password);
}

static int test_several_auth_methods(ssh_session session)
{
  int rc = ssh_userauth_none(session, NULL);
  if (rc != SSH_AUTH_SUCCESS) {
      return rc;
  }

  int method = ssh_userauth_list(session, NULL);
#if 0
  if (method & SSH_AUTH_METHOD_NONE)
  { // For the source code of function authenticate_none(),
    // refer to the corresponding example
    rc = authenticate_none(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
#endif
  if (method & SSH_AUTH_METHOD_PUBLICKEY)
  { // For the source code of function authenticate_pubkey(),
    // refer to the corresponding example
    rc = authenticate_pubkey(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
#if 0
  if (method & SSH_AUTH_METHOD_INTERACTIVE)
  { // For the source code of function authenticate_kbdint(),
    // refer to the corresponding example
    rc = authenticate_kbdint(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
#endif
  if (method & SSH_AUTH_METHOD_PASSWORD)
  { // For the source code of function authenticate_password(),
    // refer to the corresponding example
    rc = authenticate_password(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
  return SSH_AUTH_ERROR;
}

int ssh_open_url(url_t* urlp)
{
	ftp->session = ssh_new();
	if (!ftp->session)
		return -1;

	/* set log level */
	if (ftp_get_verbosity() == vbDebug)
	{
		int verbosity = SSH_LOG_PROTOCOL;
		ssh_options_set(ftp->session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	}

	/* set host name and port */
	ssh_options_set(ftp->session, SSH_OPTIONS_HOST, urlp->hostname);
	if (urlp->port)
		ssh_options_set(ftp->session, SSH_OPTIONS_PORT, &urlp->port);
	
	/* parse .ssh/config */
	int r = ssh_options_parse_config(ftp->session, NULL); 
	if (r != SSH_OK)
	{
		ftp_err("Failed to parse ssh config: %s\n", ssh_get_error(ftp->session));
		ssh_free(ftp->session);
		ftp->session = NULL;
		return r;
	}
	
	/* get user name */
	char* default_username = NULL;
	ssh_options_get(ftp->session, SSH_OPTIONS_USER, &default_username);
	r = get_username(urlp, default_username, false);
	ssh_string_free_char(default_username);
	if (r)
	{
		ssh_free(ftp->session);
		ftp->session = NULL;
		return r;
	}

	/* set user name */
	ssh_options_set(ftp->session, SSH_OPTIONS_USER, urlp->username);

	/* connect to server */
	r = ssh_connect(ftp->session);
	if (r != SSH_OK)
	{
		ftp_err("Couldn't initialise connection to server: %s\n", ssh_get_error(ftp->session));
		ssh_free(ftp->session);
		ftp->session = NULL;
		return r;
	}

	/* verify server */
	if (verify_knownhost(ftp->session))
	{
		ssh_disconnect(ftp->session);
		ssh_free(ftp->session);
		ftp->session = NULL;
		return -1;
	}

	/* authenticate user */
	r = test_several_auth_methods(ftp->session);
	if (r != SSH_OK)
	{
		ftp_err("Authentication failed: %s\n", ssh_get_error(ftp->session));
		ssh_disconnect(ftp->session);
		ssh_free(ftp->session);
		ftp->session = NULL;
		return -1;
	}

	ftp->ssh_version = ssh_get_openssh_version(ftp->session);
	if (!ftp->ssh_version) {
		ftp_err("Couldn't initialise connection to server\n");
		return -1;
	}

	ftp->sftp_session = sftp_new(ftp->session);
	if (!ftp->sftp_session)
	{
		ftp_err("Couldn't initialise ftp subsystem: %s\n", ssh_get_error(ftp->session));
		ssh_disconnect(ftp->session);
		ssh_free(ftp->session);
		ftp->session = NULL;
		return -1;
	}

	r = sftp_init(ftp->sftp_session);
	if (r != SSH_OK)
	{
		ftp_err("Couldn't initialise ftp subsystem: %s\n", ssh_get_error(ftp->sftp_session));
		sftp_free(ftp->sftp_session);
		ftp->sftp_session = NULL;
		ssh_disconnect(ftp->session);
		ssh_free(ftp->session);
		ftp->session = NULL;
		return r;
	}

	ftp->connected = true;
	ftp->loggedin = true;

	free(ftp->homedir);
	ftp->homedir = ftp_getcurdir();

	url_destroy(ftp->url);
	ftp->url = url_clone(urlp);

	free(ftp->curdir);
	ftp->curdir = xstrdup(ftp->homedir);
	free(ftp->prevdir);
	ftp->prevdir = xstrdup(ftp->homedir);
	if (ftp->url->directory)
		ftp_chdir(ftp->url->directory);

	ftp_get_feat();
	return 0;
}

char *ssh_getcurdir(void)
{
	char *ret = ssh_realpath(".");
	if(!ret)
		return xstrdup("CWD?");
	stripslash(ret);
	return ret;
}

int ssh_chdir(const char *path)
{
	Attrib *aa;
	char *p = ftp_path_absolute(path);
	bool isdir = false;

	/* First check if this file is cached and is a directory, else we
	 * need to stat the file to see if it really is a directory
	 */

	stripslash(p);
	isdir = (ftp_cache_get_directory(p) != 0);
	if(!isdir) {
		rfile *rf = ftp_cache_get_file(p);
		isdir = (rf && risdir(rf));
	}
	if(!isdir) {
		if ((aa = ssh_stat(p)) == 0) {
			free(p);
			return -1;
		}
		if (!(aa->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)) {
			ftp_err("Can't change directory: Can't check target");
			free(p);
			return -1;
		}
		if (!S_ISDIR(aa->perm)) {
			ftp_err("%s: not a directory\n", p);
			free(p);
			return -1;
		}
	}
	ftp_update_curdir_x(p);

	return 0;
}

int ssh_cdup(void)
{
	return ssh_chdir("..");
}

int ssh_mkdir_verb(const char *path, verbose_t verb)
{
	char* bspath = ftp_path_absolute(path);
	stripslash(abspath);

	int r = sftp_mkdir(ftp->sftp_session, abspath, S_IRWXU);
	if (rc != SSH_OK && sftp_get_error(ftp->sftp_session) != SSH_FX_FILE_ALREADY_EXISTS)
	{
		ftp_err("Couldn't create directory: %s\n", ssh_get_error(session));
		free(abspath);
		return rc;
	}

	ftp_cache_flush_mark_for(abspath);
	free(abspath);
	return 0;
}

int ssh_rmdir(const char *path)
{
	char* bspath = ftp_path_absolute(path);
	stripslash(abspath);

	int r = sftp_rmdir(ftp->sftp_session, abspath);
	if (rc != SSH_OK && sftp_get_error(ftp->sftp_session) != SSH_FX_NO_SUCH_FILE)
	{
		ftp_err("Couldn't remove directory: %s\n", ssh_get_error(session));
		free(abspath);
		return rc;
	}

	ftp_cache_flush_mark(abspath);
	ftp_cache_flush_mark_for(abspath);
	free(abspath);
	return 0;
}

int ssh_unlink(const char *path)
{
	u_int status, id;

	id = ftp->ssh_id++;
	ssh_send_string_request(id, SSH2_FXP_REMOVE, path, strlen(path));
	status = ssh_get_status(id);
	if(status != SSH2_FX_OK) {
/*		ftp_err("Couldn't delete file: %s\n", fx2txt(status));*/
		ftp->code = ctError;
		ftp->fullcode = 500;
		return -1;
	} else
		ftp_cache_flush_mark_for(path);

	return 0;
}

/* FIXME: path must be absolute (check)
 */
int ssh_chmod(const char *path, const char *mode)
{
	Attrib a;

	attrib_clear(&a);
	a.flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
	a.perm = strtoul(mode, 0, 8);
	if(ssh_setstat(path, &a) == -1)
		return -1;
	ftp_cache_flush_mark_for(path);
	return 0;
}

int ssh_idle(const char *idletime)
{
	return 0;
}

int ssh_noop(void)
{
	return 0;
}

int ssh_help(const char *arg)
{
	return 0;
}

unsigned long ssh_filesize(const char *path)
{
	Attrib *a = ssh_stat(path);
	return a ? a->size : 0;
}

rdirectory *ssh_read_directory(const char *path)
{
	rdirectory *rdir;
	int i;
	SFTP_DIRENT **dir;
	char *p = ftp_path_absolute(path);
	stripslash(p);

	if(ssh_readdir(p, &dir) != 0) {
		free(p);
		return 0;
	}

	rdir = rdir_create();

	ftp_trace("*** start parsing directory listing ***\n");

	for(i = 0; dir[i]; i++) {
		rfile *rf;
		char *e, *cf = dir[i]->longname;

		ftp_trace("%s\n", dir[i]->longname);

		rf = rfile_create();

		rf->perm = perm2string(dir[i]->a.perm);

		e = strqsep(&cf, ' ');  /* skip permissions */
		e = strqsep(&cf, ' ');  /* nhl? */
/*		if(ftp->ssh_version > 2) {*/
			rf->nhl = atoi(e);
/*		} else*/
/*			rf->nhl = 0;*/
#if 1
		e = strqsep(&cf, ' ');
		rf->owner = xstrdup(e);
		e = strqsep(&cf, ' ');
		rf->group = xstrdup(e);
#else
		asprintf(&rf->owner, "%d", dir[i]->a.uid);
		asprintf(&rf->group, "%d", dir[i]->a.gid);
#endif

		asprintf(&rf->path, "%s/%s", p, dir[i]->filename);
		rf->mtime = dir[i]->a.mtime;
		if(rf->mtime == 0) {
			char *m, *d, *y;
			while(e && month_number(e) == -1)
				e = strqsep(&cf, ' ');
			if(e) {
				m = e;
				d = strqsep(&cf, ' ');
				y = strqsep(&cf, ' ');
				ftp_trace("parsing time: m:%s d:%s y:%s\n", m, d, y);
				rfile_parse_time(rf, m, d, y);
			}
		}
		rf->date = time_to_string(rf->mtime);
		rf->size = dir[i]->a.size;
		rfile_parse_colors(rf);

		rf->link = 0;
		if(rislink(rf) && ftp->ssh_version > 2)
			rf->link = ssh_readlink(rf->path);

		list_additem(rdir->files, (void *)rf);
	}
	ftp_trace("*** end parsing directory listing ***\n");

	ssh_free_dirents(dir);

	rdir->path = p;

	ftp_trace("added directory '%s' to cache\n", p);
	list_additem(ftp->cache, rdir);

	return rdir;
}

int ssh_rename(const char *oldname, const char *newname)
{
	Buffer msg;
	u_int status, id;
	char *on, *nn;

	buffer_init(&msg);

	on = ftp_path_absolute(oldname);
	nn = ftp_path_absolute(newname);
	stripslash(on);
	stripslash(nn);

	/* Send rename request */
	id = ftp->ssh_id++;
	buffer_put_char(&msg, SSH2_FXP_RENAME);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, on);
	buffer_put_cstring(&msg, nn);
	ssh_cmd( &msg);
	buffer_free(&msg);

	status = ssh_get_status(id);
	if(status != SSH2_FX_OK) {
		ftp_err("Couldn't rename file \"%s\" to \"%s\": %s\n",
				on, nn, fx2txt(status));
		free(on);
		free(nn);
		return -1;
	}

	ftp_cache_flush_mark_for(on);
	ftp_cache_flush_mark_for(nn);
	free(on);
	free(nn);
	return 0;
}

time_t ssh_filetime(const char *filename)
{
	Attrib *a = ssh_stat(filename);
	return a ? a->mtime : -1;
}

int ssh_list(const char *cmd, const char *param, FILE *fp)
{
	ftp_err("ssh_list() not implemented yet\n");

	return -1;
}

void ssh_pwd(void)
{
	printf("%s\n", ftp->curdir);
}

int ssh_do_receive(const char *infile, FILE *fp, getmode_t mode,
				   ftp_transfer_func hookf)
{
	int r;
	rfile *f;
	u_int64_t offset = ftp->restart_offset;

	ftp->ti.size = ftp->ti.restart_size = offset;
	ftp->restart_offset = 0L;

	f = ftp_cache_get_file(infile);
	if(f)
		ftp->ti.total_size = f->size;
	else
		ftp->ti.total_size = ssh_filesize(infile);

	r = ssh_recv_binary(infile, fp, hookf, offset);

	transfer_finished();

	return (r == 0 && !ftp->ti.ioerror && !ftp->ti.interrupted) ? 0 : -1;
}

int ssh_send(const char *path, FILE *fp, putmode_t how,
			 transfer_mode_t mode, ftp_transfer_func hookf)
{
	int r;
	long offset = ftp->restart_offset;
	char *p;

	reset_transfer_info();
	ftp->ti.size = ftp->ti.restart_size = offset;
	ftp->restart_offset = 0L;

	ftp->ti.transfer_is_put = true;

	if(how == putUnique) {
		ftp_err("Unique put with SSH not implemented yet\n");
		return -1;
#if 0
		/* try to figure out remote filename */
		char *e = strstr(ftp->reply, " for ");
		if(e) {
			int l;
			e += 5;
			l = strlen(e);
			if(l) {
				free(ftp->ti.local_name);
				if(*e == '\'')
					ftp->ti.local_name = xstrndup(e+1, l-3);
				else
					ftp->ti.local_name = xstrndup(e, l-1);
				ftp_trace("parsed unique filename as '%s'\n",
						  ftp->ti.local_name);
			}
		}
#endif
	}

	p = ftp_path_absolute(path);
	stripslash(p);

	ftp_cache_flush_mark_for(p);

	if(how == putAppend) {
		ftp_set_tmp_verbosity(vbNone);
		offset = ftp_filesize(p);
	}

	r = ssh_send_binary(p, fp, hookf, offset);
	free(p);

	transfer_finished();

	return 0;
}

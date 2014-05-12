/*
 * ssh_cmd.c -- sftp support via libssh.
 *
 * Yet Another FTP Client
 * Copyright (C) 2012, Sebastian Ramacher <sebastian+dev@ramacher.at>
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "ftp.h"
#include "strq.h"
#include "gvars.h"
#include "libmhe/args.h"
#include "rfile.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if defined(BUFSIZ) && BUFSIZ >= 131072
#define SSH_BUFSIZ BUFSIZ
#else
#define SSH_BUFSIZ 131072
#endif

#ifndef MIN
#define MIN(a,b) (a) < (b) ? (a) : (b)
#endif

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
      fprintf(stderr, _("Host key for server changed. It is now:\n"));
      ssh_print_hexa(_("Public key hash"), hash, hlen);
      fprintf(stderr, _("For security reasons, connection will be stopped.\n"));
      free(hash);
      return -1;

    case SSH_SERVER_FOUND_OTHER:
      fprintf(stderr, _("The host key for this server was not found but "
        "another type of key exists.\n"));
      fprintf(stderr, _("An attacker might change the default server key to "
        "confuse your client into thinking the key does not exist\n"));
      free(hash);
      return -1;

    case SSH_SERVER_FILE_NOT_FOUND:
      fprintf(stderr, _("Could not find known host file.\n"));
      fprintf(stderr, _("If you accept the host key here, the file will be "
       "automatically created.\n"));
      /* fallback to SSH_SERVER_NOT_KNOWN behavior */

    case SSH_SERVER_NOT_KNOWN:
      hexa = ssh_get_hexa(hash, hlen);
      fprintf(stderr, _("The server is unknown. Do you trust the host key?\n"));
      fprintf(stderr, _("Public key hash: %s\n"), hexa);
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
        fprintf(stderr, _("Error %s\n"), strerror(errno));
        free(hash);
        return -1;
      }
      break;

    case SSH_SERVER_ERROR:
      fprintf(stderr, _("Error %s\n"), ssh_get_error(session));
      free(hash);
      return -1;
  }

  free(hash);
  return 0;
}

static int authenticate_none(ssh_session session)
{
  return ssh_userauth_none(session, NULL);
}

static int authenticate_pubkey(ssh_session session)
{
  return ssh_userauth_autopubkey(session, NULL);
}

static int authenticate_password(ssh_session session, url_t* urlp)
{
  if (urlp->password)
    return ssh_userauth_password(session, NULL, urlp->password);

  if (!ftp->getpass_hook)
    return SSH_AUTH_ERROR;

  if (get_password(urlp, NULL, false))
    return SSH_AUTH_ERROR;

  return ssh_userauth_password(session, NULL, urlp->password);
}

static int authenticate_kbdint(ssh_session session)
{
  int rc = ssh_userauth_kbdint(session, NULL, NULL);
  while (rc == SSH_AUTH_INFO)
  {
    const char* name = ssh_userauth_kbdint_getname(session);
    const char* instruction = ssh_userauth_kbdint_getinstruction(session);
    int nprompts = ssh_userauth_kbdint_getnprompts(session);

    if (strlen(name) > 0)
      printf("%s\n", name);
    if (strlen(instruction) > 0)
      printf("%s\n", instruction);
    for (int iprompt = 0; iprompt < nprompts; iprompt++)
    {
      const char* prompt = NULL;
      char echo;

      prompt = ssh_userauth_kbdint_getprompt(session, iprompt, &echo);
      if (echo)
      {
        char buffer[128], *ptr;

        printf("%s", prompt);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
          return SSH_AUTH_ERROR;
        buffer[sizeof(buffer) - 1] = '\0';
        if ((ptr = strchr(buffer, '\n')) != NULL)
          *ptr = '\0';
        if (ssh_userauth_kbdint_setanswer(session, iprompt, buffer) < 0)
          return SSH_AUTH_ERROR;
        memset(buffer, 0, strlen(buffer));
      }
      else
      {
        if (!ftp->getpass_hook)
          return SSH_AUTH_ERROR;

        char* pw = ftp->getpass_hook(prompt);
        if (!pw)
          return SSH_AUTH_ERROR;

        rc = ssh_userauth_kbdint_setanswer(session, iprompt, pw);
        memset(pw, 0, strlen(pw));
        free(pw);
        if (rc < 0)
          return SSH_AUTH_ERROR;
      }
    }
    rc = ssh_userauth_kbdint(session, NULL, NULL);
  }
  return rc;
}

static int test_several_auth_methods(ssh_session session, url_t* urlp)
{
  int rc = ssh_userauth_none(session, NULL);
  /* if (rc != SSH_AUTH_SUCCESS) {
      return rc;
  } */

  int method = ssh_userauth_list(session, NULL);
  if (method & SSH_AUTH_METHOD_NONE)
  {
    rc = authenticate_none(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
  if (method & SSH_AUTH_METHOD_PUBLICKEY)
  {
    rc = authenticate_pubkey(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
  if (method & SSH_AUTH_METHOD_INTERACTIVE)
  {
    rc = authenticate_kbdint(session);
    if (rc == SSH_AUTH_SUCCESS) return rc;
  }
  if (method & SSH_AUTH_METHOD_PASSWORD)
  {
    rc = authenticate_password(session, urlp);
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

  /* If we have ssh options from yafcrc, load them */
  if (gvSSHOptions) {
    args_t *args = args_create();
    args_init(args, 0, NULL);
    args_push_back(args, gvSSHOptions);
    int argc = 0;
    if (ssh_options_getopt(ftp->session, &argc, args->argv) != SSH_OK) {
      ftp_err(_("Failed to load SSH options from yafcrc config (ssh_options = '%s')\n"), gvSSHOptions);
      ssh_free(ftp->session);
      ftp->session = NULL;
      return -1;
    }
    args->argc = argc;
    args_destroy(args);
  }

  /* set host name */
  ssh_options_set(ftp->session, SSH_OPTIONS_HOST, urlp->hostname);
  /* if we have port use that one */
  if (urlp->port > 0)
    ssh_options_set(ftp->session, SSH_OPTIONS_PORT, &urlp->port);

  /* parse .ssh/config */
  int r = ssh_options_parse_config(ftp->session, NULL);
  if (r != SSH_OK)
  {
    ftp_err(_("Failed to parse ssh config: %s\n"), ssh_get_error(ftp->session));
    ssh_free(ftp->session);
    ftp->session = NULL;
    return r;
  }

  /* if we have username use that one */
  if (urlp->username)
    ssh_options_set(ftp->session, SSH_OPTIONS_USER, urlp->username);

  /* connect to server */
  r = ssh_connect(ftp->session);
  if (r != SSH_OK)
  {
    ftp_err(_("Couldn't initialise connection to server: %s\n"), ssh_get_error(ftp->session));
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
  r = test_several_auth_methods(ftp->session, urlp);
  if (r != SSH_OK)
  {
    ftp_err(_("Authentication failed: %s\n"), ssh_get_error(ftp->session));
    ssh_disconnect(ftp->session);
    ssh_free(ftp->session);
    ftp->session = NULL;
    return -1;
  }

  ftp->ssh_version = ssh_get_version(ftp->session);
  if (!ftp->ssh_version) {
    ftp_err(_("Couldn't initialise connection to server\n"));
    return -1;
  }

  ftp->sftp_session = sftp_new(ftp->session);
  if (!ftp->sftp_session)
  {
    ftp_err(_("Couldn't initialise ftp subsystem: %s\n"), ssh_get_error(ftp->session));
    ssh_disconnect(ftp->session);
    ssh_free(ftp->session);
    ftp->session = NULL;
    return -1;
  }

  r = sftp_init(ftp->sftp_session);
  if (r != SSH_OK)
  {
    ftp_err(_("Couldn't initialise ftp subsystem: %s\n"), ssh_get_error(ftp->sftp_session));
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
  char *ret = sftp_canonicalize_path(ftp->sftp_session, ".");
  if(!ret)
    return xstrdup("CWD?");
  stripslash(ret);
  return ret;
}

int ssh_chdir(const char *path)
{
  char* tmp = ftp_path_absolute(path);
  char *p = sftp_canonicalize_path(ftp->sftp_session, tmp);
  free(tmp);
  if (!p)
  {
    ftp_err("%s: %s\n", path, ssh_get_error(ftp->session));
    return -1;
  }

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
  if (!isdir)
  {
    sftp_attributes attrib = sftp_stat(ftp->sftp_session, p);
    if (!attrib)
    {
      ftp_err(_("Couldn't stat directory: %s\n"), ssh_get_error(ftp->session));
      free(p);
      return -1;
    }
    if (!S_ISDIR(attrib->permissions)) {
      ftp_err(_("%s: not a directory\n"), p);
      sftp_attributes_free(attrib);
      free(p);
      return -1;
    }
    sftp_attributes_free(attrib);
  }
  ftp_update_curdir_x(p);
  free(p);

  return 0;
}

int ssh_cdup(void)
{
  return ssh_chdir("..");
}

int ssh_mkdir_verb(const char *path, verbose_t verb)
{
  char* abspath = ftp_path_absolute(path);
  stripslash(abspath);

  int rc = sftp_mkdir(ftp->sftp_session, abspath, S_IRWXU);
  if (rc != SSH_OK && sftp_get_error(ftp->sftp_session) != SSH_FX_FILE_ALREADY_EXISTS)
  {
    ftp_err(_("Couldn't create directory: %s\n"), ssh_get_error(ftp->session));
    free(abspath);
    return rc;
  }

  ftp_cache_flush_mark_for(abspath);
  free(abspath);
  return 0;
}

int ssh_rmdir(const char *path)
{
  char* abspath = ftp_path_absolute(path);
  stripslash(abspath);

  int rc = sftp_rmdir(ftp->sftp_session, abspath);
  if (rc != SSH_OK && sftp_get_error(ftp->sftp_session) != SSH_FX_NO_SUCH_FILE)
  {
    ftp_err(_("Couldn't remove directory: %s\n"), ssh_get_error(ftp->session));
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
  int rc = sftp_unlink(ftp->sftp_session, path);
  if (rc != SSH_OK && sftp_get_error(ftp->sftp_session) != SSH_FX_NO_SUCH_FILE)
  {
    ftp_err(_("Couldn't delete file: %s\n"), ssh_get_error(ftp->session));
    ftp->code = ctError;
    ftp->fullcode = 500;
    return -1;
  }
  else
    ftp_cache_flush_mark_for(path);

  return 0;
}

/* FIXME: path must be absolute (check)
 */
int ssh_chmod(const char *path, const char *mode)
{
  mode_t perm = strtoul(mode, 0, 9);
  int rc = sftp_chmod(ftp->sftp_session, path, perm);
  if (rc != SSH_OK)
  {
    ftp_err(_("Couldn't chmod file: %s\n"), ssh_get_error(ftp->session));
    return -1;
  }
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

uint64_t ssh_filesize(const char *path)
{
  sftp_attributes attrib = sftp_stat(ftp->sftp_session, path);
  if (!attrib)
    return 0;

  uint64_t res = attrib->size;
  sftp_attributes_free(attrib);
  return res;
}

rdirectory* ssh_read_directory(const char *path)
{
  char* p = ftp_path_absolute(path);
  stripslash(p);

  sftp_dir dir = sftp_opendir(ftp->sftp_session, p);
  if (!dir)
  {
    free(p);
    return NULL;
  }

  ftp_trace("*** start parsing directory listing ***\n");
  rdirectory* rdir = rdir_create();
  sftp_attributes attrib = NULL;
  while ((attrib = sftp_readdir(ftp->sftp_session, dir)) != NULL)
  {
    ftp_trace("%s\n", attrib->longname);

    rfile* rf = rfile_create();
    rf->perm = perm2string(attrib->permissions);

    rf->nhl = 0; // atoi(e);
    if (attrib->owner)
      rf->owner = xstrdup(attrib->owner);
    if (attrib->group)
      rf->group = xstrdup(attrib->group);

    if (asprintf(&rf->path, "%s/%s", p, attrib->name) == -1)
    {
      ftp_err(_("Failed to allocate memory.\n"));
      sftp_closedir(dir);
      free(p);
      rdir_destroy(rdir);
      rfile_destroy(rf);
    }
    rf->mtime = attrib->mtime;
    rf->date = time_to_string(rf->mtime);
    rf->size = attrib->size;
    rfile_parse_colors(rf);

    rf->link = NULL;
    if (rislink(rf) && ftp->ssh_version > 2)
      rf->link = sftp_readlink(ftp->sftp_session, rf->path);

    list_additem(rdir->files, (void *)rf);
    sftp_attributes_free(attrib);
  }
  ftp_trace("*** end parsing directory listing ***\n");

  if (!sftp_dir_eof(dir))
  {
    ftp_err(_("Couldn't list directory: %s\n"), ssh_get_error(ftp->session));
    sftp_closedir(dir);
    free(p);
    rdir_destroy(rdir);
    return NULL;
  }

  sftp_closedir(dir);
  rdir->path = p;
  rdir_sort(rdir);
  ftp_trace("added directory '%s' to cache\n", p);
  list_additem(ftp->cache, rdir);

  return rdir;
}

int ssh_rename(const char *oldname, const char *newname)
{
  char* on = ftp_path_absolute(oldname);
  char* nn = ftp_path_absolute(newname);
  stripslash(on);
  stripslash(nn);

  int rc = sftp_rename(ftp->sftp_session, on, nn);
  if (rc != SSH_OK)
  {
    ftp_err(_("Couldn't rename file '%s' to '%s': %s\n"),
        on, nn, ssh_get_error(ftp->session));
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
  sftp_attributes attrib = sftp_stat(ftp->sftp_session, filename);
  if (!attrib)
    return -1;

  time_t res = attrib->mtime; /* mtime 64? */
  sftp_attributes_free(attrib);
  return res;
}

int ssh_list(const char *cmd, const char *param, FILE *fp)
{
  ftp_err(_("ssh_list() not implemented yet\n"));

  return -1;
}

void ssh_pwd(void)
{
  printf("%s\n", ftp->curdir);
}

static int do_scp_read(ssh_scp scp, const char* infile, FILE* fp,
                       transfer_mode_t mode, ftp_transfer_func hookf)
{
  time_t then = time(NULL) - 1;
  ftp_set_close_handler();

  if (hookf)
    hookf(&ftp->ti);
  ftp->ti.begin = false;

  int rc = ssh_scp_pull_request(scp);
  if (rc != SSH_SCP_REQUEST_NEWFILE)
  {
    ftp_trace("Failed to start scp download: %s\n",
              ssh_get_error(ftp->session));
    ssh_scp_close(scp);
    ssh_scp_free(scp);
    return -2;
  }

  size_t size = ssh_scp_request_get_size(scp);
  ssh_scp_accept_request(scp);

  /* read file */
  char buffer[SSH_BUFSIZ];
  int r = 0;
  while (size && (r = ssh_scp_read(scp, buffer, MIN(SSH_BUFSIZ, size))) != SSH_ERROR)
  {
    if (ftp_sigints() > 0)
    {
      ftp_trace("break due to sigint\n");
      break;
    }

    errno = 0;
    if (fwrite(buffer, r, 1, fp) != 1)
    {
      ftp_err(_("Error while writing to file: %s\n"), strerror(errno));
      ssh_scp_close(scp);
      ssh_scp_free(scp);
      return -1;
    }

    ftp->ti.size += r;
    if (hookf)
    {
      time_t now = time(NULL);
      if (now > then)
      {
        hookf(&ftp->ti);
        then = now;
      }
    }
    size -= r;
  }

  if (r == SSH_ERROR)
  {
    ftp_err(_("Error while reading from file: %s\n"), ssh_get_error(ftp->session));
    r = -1;
  }
  else
  {
    r = ssh_scp_pull_request(scp);
    if (r != SSH_SCP_REQUEST_EOF)
      ftp_err(_("Unexpected request: %s %lu\n"), ssh_get_error(ftp->session), size);
    else
      r = 0;
  }

  ssh_scp_close(scp);
  ssh_scp_free(scp);
  return r;
}

static int do_read(const char* infile, FILE* fp, transfer_mode_t mode,
                   ftp_transfer_func hookf, uint64_t offset)
{
  if (gvSSHTrySCP && !offset)
  {
    char* escaped = backslash_quote(infile);
    /* try to set up a scp connection */
    ssh_scp scp = ssh_scp_new(ftp->session, SSH_SCP_READ, escaped);
    free(escaped);
    if (scp != NULL)
    {
      int rc = ssh_scp_init(scp);
      if (rc == SSH_OK)
      {
        rc = do_scp_read(scp, infile, fp, mode, hookf);
        if (rc != -2)
          return rc;
      }
      else
        ssh_scp_free(scp);
    }
  }

  time_t then = time(NULL) - 1;
  ftp_set_close_handler();

  if (hookf)
    hookf(&ftp->ti);
  ftp->ti.begin = false;

  /* check if remote file is not a directory */
  sftp_attributes attrib = sftp_stat(ftp->sftp_session, infile);
  if (!attrib)
    return -1;

  if (S_ISDIR(attrib->permissions))
  {
    ftp_err(_("Cannot download a directory: %s\n"), infile);
    sftp_attributes_free(attrib);
    return -1;
  }
  sftp_attributes_free(attrib);

  /* open remote file */
  sftp_file file = sftp_open(ftp->sftp_session, infile, O_RDONLY, 0);
  if (!file)
  {
    ftp_err(_("Cannot open file for reading: %s\n"), ssh_get_error(ftp->session));
    return -1;
  }

  /* seek to offset */
  int r = sftp_seek64(file, offset);
  if (r != SSH_OK)
  {
    ftp_err(_("Failed to seek: %s\n"), ssh_get_error(ftp->session));
    sftp_close(file);
    return -1;
  }

  /* read file */
  char buffer[SSH_BUFSIZ];
  ssize_t nbytes = 0;
  while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0)
  {
    if (ftp_sigints() > 0)
    {
      ftp_trace("break due to sigint\n");
      break;
    }

    errno = 0;
    if (fwrite(buffer, nbytes, 1, fp) != 1)
    {
      ftp_err(_("Error while writing to file: %s\n"), strerror(errno));
      ftp->ti.ioerror = true;
      sftp_close(file);
      return -1;
    }

    ftp->ti.size += nbytes;
    if (hookf)
    {
      time_t now = time(NULL);
      if (now > then)
      {
        hookf(&ftp->ti);
        then = now;
      }
    }
  }

  if (nbytes < 0)
  {
    ftp_err(_("Error while reading from file: %s\n"), ssh_get_error(ftp->session));
    r = -1;
  }

  sftp_close(file);
  return r;
}

int ssh_do_receive(const char *infile, FILE *fp, transfer_mode_t mode,
                   ftp_transfer_func hookf)
{
  uint64_t offset = ftp->restart_offset;
  ftp->ti.size = ftp->ti.restart_size = offset;
  ftp->restart_offset = 0L;

  rfile* f = ftp_cache_get_file(infile);
  if (f)
    ftp->ti.total_size = f->size;
  else
    ftp->ti.total_size = ssh_filesize(infile);

  int r = do_read(infile, fp, mode, hookf, offset);
  transfer_finished();

  return (r == 0 && !ftp->ti.ioerror && !ftp->ti.interrupted) ? 0 : -1;
}

static int do_scp_write(ssh_scp scp, const char* path, FILE* fp,
                        ftp_transfer_func hookf)
{
  time_t then = time(NULL) - 1;
  ftp_set_close_handler();

  if (hookf)
    hookf(&ftp->ti);
  ftp->ti.begin = false;

  struct stat sb;
  errno = 0;
  if (fstat(fileno(fp), &sb) == -1)
  {
    ftp_err(_("Couldn't fstat local file: %s\n"), strerror(errno));
    ssh_scp_free(scp);
    return -1;
  }

  int rc = ssh_scp_push_file(scp, path, sb.st_size, sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
  if (rc != SSH_OK)
  {
    ftp_err(_("Failed to start scp upload: %s\n"),
            ssh_get_error(ftp->session));
    ssh_scp_close(scp);
    ssh_scp_free(scp);
    return -2;
  }

  /* read file */
  char buffer[SSH_BUFSIZ];
  ssize_t nbytes = 0;
  errno = 0;
  while ((nbytes = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0)
  {
    if (ftp_sigints() > 0)
    {
      ftp_trace("break due to sigint\n");
      break;
    }

    rc = ssh_scp_write(scp, buffer, nbytes);
    if (rc != SSH_OK)
    {
      ftp_err(_("Error while writing to file: %s\n"), ssh_get_error(ftp->session));
      ssh_scp_close(scp);
      ssh_scp_free(scp);
      return -1;
    }

    ftp->ti.size += nbytes;
    if (hookf)
    {
      time_t now = time(NULL);
      if (now > then)
      {
        hookf(&ftp->ti);
        then = now;
      }
    }
    errno = 0;
  }

  if (ferror(fp))
  {
    ftp_err(_("Failed to read from file: %s\n"), strerror(errno));
    rc = -1;
  }

  ssh_scp_close(scp);
  ssh_scp_free(scp);
  return rc;
}

static int do_write(const char* path, FILE* fp, ftp_transfer_func hookf,
                    uint64_t offset)
{
  /* try to set up a scp connection */
  if (gvSSHTrySCP && !offset)
  {
    ssh_scp scp = ssh_scp_new(ftp->session, SSH_SCP_WRITE, path);
    if (scp != NULL)
    {
      int rc = ssh_scp_init(scp);
      if (rc == SSH_OK)
      {
        rc = do_scp_write(scp, path, fp, hookf);
        if (rc != -2)
          return rc;
      }
      else
        ssh_scp_free(scp);
    }
  }

  time_t then = time(NULL) - 1;
  ftp_set_close_handler();

  if (hookf)
    hookf(&ftp->ti);
  ftp->ti.begin = false;

  struct stat sb;
  errno = 0;
  if (fstat(fileno(fp), &sb) == -1)
  {
    ftp_err(_("Couldn't fstat local file: %s\n"), strerror(errno));
    return -1;
  }

  /* open remote file */
  sftp_file file = sftp_open(ftp->sftp_session, path, O_WRONLY | O_CREAT |
      (offset == 0u ? O_TRUNC : 0), sb.st_mode);
  if (!file)
  {
    ftp_err(_("Cannot open file for writing: %s\n"), ssh_get_error(ftp->session));
    return -1;
  }

  /* seek to offset */
  int r = sftp_seek64(file, offset);
  if (r != SSH_OK)
  {
    ftp_err(_("Failed to seek: %s\n"), ssh_get_error(ftp->session));
    sftp_close(file);
    return -1;
  }

  /* read file */
  char buffer[SSH_BUFSIZ];
  ssize_t nbytes = 0;
  errno = 0;
  while ((nbytes = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0)
  {
    if (ftp_sigints() > 0)
    {
      ftp_trace("break due to sigint\n");
      break;
    }

    ssize_t nwritten = sftp_write(file, buffer, nbytes);
    if (nwritten != nbytes)
    {
      ftp_err(_("Error while writing to file: %s\n"), ssh_get_error(ftp->session));
      sftp_close(file);
      return -1;
    }

    ftp->ti.size += nbytes;
    if (hookf)
    {
      time_t now = time(NULL);
      if (now > then)
      {
        hookf(&ftp->ti);
        then = now;
      }
    }
    errno = 0;
  }

  if (ferror(fp))
  {
    ftp_err(_("Failed to read from file: %s\n"), strerror(errno));
    r = -1;
  }

  sftp_close(file);
  return r;

}

int ssh_send(const char *path, FILE *fp, putmode_t how,
       transfer_mode_t mode, ftp_transfer_func hookf)
{
  uint64_t offset = ftp->restart_offset;

  reset_transfer_info();
  ftp->ti.size = ftp->ti.restart_size = offset;
  ftp->restart_offset = 0L;

  ftp->ti.transfer_is_put = true;

  if(how == putUnique) {
    ftp_err(_("Unique put with SSH not implemented yet\n"));
    return -1;
  }

  char* p = ftp_path_absolute(path);
  stripslash(p);
  ftp_cache_flush_mark_for(p);

  if(how == putAppend) {
    ftp_set_tmp_verbosity(vbNone);
    offset = ftp_filesize(p);
  }

  int r = do_write(p, fp, hookf, offset);
  free(p);

  transfer_finished();
  return r;
}

/*
 * ssh_cmd.h --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _ssh_cmd_h_
#define _ssh_cmd_h_

/* ssh_cmd.c */
int ssh_open_url(url_t *urlp);
char *ssh_getcurdir(void);
int ssh_chdir(const char *path);
int ssh_cdup(void);
int ssh_mkdir_verb(const char *path, verbose_t verb);
int ssh_rmdir(const char *path);
int ssh_unlink(const char *path);
int ssh_chmod(const char *path, const char *mode);
int ssh_idle(const char *idletime);
int ssh_noop(void);
int ssh_help(const char *arg);
unsigned long ssh_filesize(const char *path);
rdirectory *ssh_read_directory(const char *path);
int ssh_rename(const char *oldname, const char *newname);
time_t ssh_filetime(const char *filename);
int ssh_list(const char *cmd, const char *param, FILE *fp);
int ssh_receive(const char *path, FILE *fp,
				transfer_mode_t mode, ftp_transfer_func hookf);
void ssh_pwd(void);
int ssh_do_receive(const char *infile, FILE *fp, getmode_t mode,
				   ftp_transfer_func hookf);
int ssh_send(const char *path, FILE *fp, putmode_t how,
			 transfer_mode_t mode, ftp_transfer_func hookf);

#endif

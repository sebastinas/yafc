/* redir.h -- 
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

static FILE *redir_fp = 0;
static bool ispipe = false;
static int oldout = -1;
static int olderr = -1;

bool reject_ampersand(char *cmd)
{
	char *e, *f;

	/* look for and reject '&' in command
	 * (asynchronous execution)
	 * allow '&&'
	 */
	e = cmd;
	while(e && *e && (f=strchr(e, '&')) != 0) {
		if(f == 0)
			break;;
		if(f[1] != '&') {
			fprintf(stderr, _("Asynchronous mode NOT supported (found '&' in command)\n"));
			return true;
		} else {
			e = f+1;
			if(!*e)
				break;
			e++;
		}
	}
	return false;
}

typedef enum {REDIR_PIPE, REDIR_WRITE, REDIR_APPEND} redir_type;

int open_redirection(char *cmd)
{
	redir_type type;
	bool errtoo = false;

	redir_fp = 0;

	if(cmd[0] == '|') {
		type = REDIR_PIPE;
	} else if(cmd[0] == '>') {
		if(cmd[1] == '>') {
			cmd++;
			type = REDIR_APPEND;
		} else
			type = REDIR_WRITE;
	} else
		return -1;

	if(!*cmd) {
		fprintf(stderr, _("missing redirection argument\n"));
		return -1;
	}

	cmd++;
	if(*cmd == '&') {
		errtoo = true;
		cmd++;
	}
	
	while(*cmd == ' ')
		cmd++;

	if(!*cmd) {
		fprintf(stderr, _("missing redirection argument\n"));
		return -1;
	}

	if(reject_ampersand(cmd))
		return -1;

	if(type == REDIR_PIPE) {
		redir_fp = popen(cmd, "w");
		ispipe = true;
	} else  {
		redir_fp = fopen(cmd, type == REDIR_WRITE ? "w" : "a");
		ispipe = false;
	}

	if(redir_fp == 0) {
		perror(cmd);
		return -1;
	}

	oldout = dup(fileno(stdout));
	if(oldout == -1) {
		perror("dup");
		fclose(redir_fp);
		redir_fp = 0;
		return -1;
	}

	if(dup2(fileno(redir_fp), fileno(stdout)) == -1) {
		perror("dup2");
		fclose(redir_fp);
		redir_fp = 0;
		return -1;
	}

	if(errtoo) {
		olderr = dup(fileno(stderr));
		if(olderr == -1) {
			perror("dup");
			fclose(redir_fp);
			if(dup2(oldout, fileno(stdout)) == -1)
				perror("dup2");
			redir_fp = 0;
			oldout = -1;
			olderr = -1;
			return -1;
		}

		if(dup2(fileno(redir_fp), fileno(stderr)) == -1) {
			perror("dup2");
			fclose(redir_fp);
			redir_fp = 0;
			if(dup2(oldout, fileno(stdout)) == -1)
				perror("dup2");
			oldout = -1;
			olderr = -1;
			return -1;
		}
	}
	return 0;
}

int close_redirection(void)
{
	if(redir_fp == 0)
		return -1;

	if(oldout == -1)
		fprintf(stderr, _("Error, cannot restore stdout!\n"));
	else {
		if(dup2(oldout, fileno(stdout)) == -1)
			perror("dup2");
		oldout = -1;
	}
	if(olderr != -1) {
		if(dup2(olderr, fileno(stderr)) == -1)
			perror("dup2");
		olderr = -1;
	}

	if(ispipe)
		pclose(redir_fp);
	else
		fclose(redir_fp);
	redir_fp = 0;
	return 0;
}

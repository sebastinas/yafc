/* $Id: cmd.h,v 1.3 2001/05/12 18:44:37 mhe Exp $
 *
 * cmd.h -- read and execute commands, this is the main loop
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _cmd_h_included
#define _cmd_h_included

#include "args.h"

#define CMD_AMBIGUOUS (cmd_t *)-1

typedef void (*vfunc)(int argc, char **argv);

typedef struct {
    char *cmd;
	bool needconnect, needlogdin;
	bool auto_unquote;
	int cpltype;
	char *hint;
	vfunc func;
} cmd_t;

/* in commands.c */
cmd_t *find_cmd(const char *name);
cmd_t *find_func(const char *cmd, bool print_error);
int rearrange_redirections(args_t *args);

void command_loop(void);
void exit_yafc(void);

extern cmd_t cmds[];

#endif

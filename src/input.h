/* input.h -- string input and readline stuff
 * 
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998-2001 martin HedenfaLk
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

#ifndef _input_h_included
#define _input_h_included

#include "args.h"

#define HISTORY_FILENAME ".yafc_history"

#define ASKYES 1
#define ASKNO 2
#define ASKCANCEL 4
#define ASKALL 8
#define ASKUNIQUE 16
#define ASKRESUME 32

extern bool readline_running;

char *getpass_hook(const char *prompt);
char *getuser_hook(const char *prompt);

void input_init(void);
char *input_read_string(const char *prompt);
int input_read_args(args_t **args, const char *prompt);
void input_save_history(void);
void input_redisplay_prompt(void);
int ask(int opt, int def, const char *prompt, ...);

char *make_dequoted_filename(const char *text, int quote_char);

#endif

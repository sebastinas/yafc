/* $Id: args.h,v 1.4 2001/05/21 21:47:55 mhe Exp $
 *
 * args.h -- handles command arguments
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _args_h_included
#define _args_h_included

typedef struct args_t
{
	unsigned int argc;
	char **argv;
} args_t;

args_t *args_create(void);
void args_destroy(args_t *args);
void args_clear(args_t *args);
void args_init(args_t *args, int argc, char **argv);
void args_init2(args_t *args, int argc, char **argv, unsigned int first);
void args_init3(args_t *args, int argc, char **argv, unsigned int first,
				unsigned int last);
char *args_cat(int argc, char **argv, unsigned int first);
char *args_cat2(const args_t *args, unsigned int first);
void args_add_args(args_t *args, const args_t *add_args);
void args_add_args2(args_t *args, const args_t *add_args, unsigned int first);
void args_add_args3(args_t *args, const args_t *add_args, unsigned int first,
					unsigned int last);
void args_del(args_t *args, unsigned int first, unsigned int n);
void args_add_null(args_t *args);
void args_push_back(args_t *args, const char *str);
void args_push_front(args_t *args, const char *str);
void args_remove_empty(args_t *args);
void args_unquote(args_t *args);
void args_insert_string(args_t *args, unsigned int index, const char *str);
void args_insert_args(args_t *args, unsigned int index, const args_t *insargs,
					  unsigned int first, unsigned int last);

#endif

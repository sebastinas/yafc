/* args.h -- handles command arguments
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
void args_init3(args_t *args, int argc, char **argv, unsigned int first, unsigned int last);
char *args_cat(int argc, char **argv, unsigned int first);
char *args_cat2(const args_t *args, unsigned int first);
void args_add_args(args_t *args, const args_t *add_args);
void args_add_args2(args_t *args, const args_t *add_args, unsigned int first);
void args_add_args3(args_t *args, const args_t *add_args, unsigned int first, unsigned int last);
void args_del(args_t *args, unsigned int first, unsigned int n);
void args_push_back(args_t *args, const char *str);
void args_push_front(args_t *args, const char *str);
void args_remove_empty(args_t *args);
void args_unquote(args_t *args);
void args_insert_string(args_t *args, unsigned int index, const char *str);
void args_insert_args(args_t *args, unsigned int index,
					  const args_t *insargs, unsigned int first, unsigned int last);

#endif

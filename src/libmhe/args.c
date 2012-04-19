/* $Id: args.c,v 1.5 2003/07/12 10:25:42 mhe Exp $
 *
 * args.c -- handles command arguments
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "args.h"
#include "strq.h"

args_t *args_create(void)
{
	args_t *args = (args_t *)xmalloc(sizeof(args_t));
	return args;
}

void args_destroy(args_t *args)
{
	args_clear(args);
	free(args);
}

void args_clear(args_t *args)
{
	if(!args)
		return;

	args_del(args, 0, args->argc);
	free(args->argv);
	args->argv = 0;
	args->argc = 0;
}

static char **alloc_argv(unsigned n)
{
	return (char **)xmalloc(n * sizeof(char *));
}

void args_init3(args_t *args, int argc, char **argv, unsigned int first,
				unsigned int last)
{
	int i;

	last++;
	if(last > argc)
		last = argc;
	if(last <= first)
		return;

	args_clear(args);

	if(last == first)
		return;

	args->argv = alloc_argv(last - first);

	for(i=first; i<last; i++)
		args->argv[args->argc++] = xstrdup(argv[i]);
}

void args_init2(args_t *args, int argc, char **argv, unsigned int first)
{
	args_init3(args, argc, argv, first, argc);
}

void args_init(args_t *args, int argc, char **argv)
{
	args_init2(args, argc, argv, 0);
}

void args_del(args_t *args, unsigned int first, unsigned int n)
{
	unsigned int i;
	unsigned int deleted = 0;

	if(!args)
		return;

	for(i=first; deleted<n && i<args->argc; i++, deleted++)
		free(args->argv[i]);

	for(i=first+n; i<args->argc; i++) {
		args->argv[i-n] = args->argv[i];
		args->argv[i] = 0;
	}

	args->argc -= deleted;
}

char *args_cat(int argc, char **argv, unsigned int first)
{
	int i;
	char *e;
	size_t s = 0;

	for(i=first; i<argc; i++)
		s += strlen(argv[i]) + 1;
	e = (char *)xmalloc(s);
	for(i=first; i<argc; i++) {
		strcat(e, argv[i]);
		if(i+1 < argc)
			strcat(e, " ");
	}
	return e;
}

char *args_cat2(const args_t *args, unsigned int first)
{
	return args_cat(args->argc, args->argv, first);
}

static args_t *split_args(const char *str)
{
	args_t *args = args_create();
	char *c, *orgc;
	char *e;

	c = orgc = xstrdup(str);

	args->argv = alloc_argv(strqnchr(str, ' ')+1);
	/* allocated argv might be way too big if there are many
	 * adjacent spaces in str */

	while(c && (e = strqsep(&c, ' ')) != 0)
		args->argv[args->argc++] = xstrdup(e);

	args->argv = xrealloc(args->argv, args->argc * sizeof(char *));

	free(orgc);
	return args;
}

void args_add_args3(args_t *args, const args_t *add_args, unsigned int first,
					unsigned int last)
{
	int i;
	char **argv;
	int argc = 0;

	last++;
	if(last > add_args->argc)
		last = add_args->argc;

	if(last <= first)
		return;

	argv = alloc_argv(args->argc + (last - first));

	/* copy orig args */
	for(i=0; i<args->argc; i++)
		argv[argc++] = args->argv[i];

	/* append add_args */
	for(i=first; i <last; i++)
		argv[argc++] = xstrdup(add_args->argv[i]);

	free(args->argv);
	args->argv = argv;
	args->argc = argc;
}

void args_add_args2(args_t *args, const args_t *add_args, unsigned int first)
{
	args_add_args3(args, add_args, first, add_args->argc);
}

void args_add_args(args_t *args, const args_t *add_args)
{
	args_add_args2(args, add_args, 0);
}

void args_add_null(args_t *args)
{
	args_t *add_args = args_create();
	add_args->argv = alloc_argv(1);
	add_args->argv[0] = 0;
	add_args->argc = 1;
	args_add_args(args, add_args);
	args_destroy(add_args);
}

void args_push_back(args_t *args, const char *str)
{
	args_t *add_args = split_args(str);
	args_add_args(args, add_args);
	args_destroy(add_args);
}

void args_push_front(args_t *args, const char *str)
{
	args_t *a = split_args(str);
	args_add_args(a, args);
	args_clear(args);
	args->argc = a->argc;
	args->argv = a->argv;
	free(a);
}

/* removes zero-length arguments
 */
void args_remove_empty(args_t *args)
{
	int i;

	for(i=0;i<args->argc;) {
		if(!args->argv[i] || strlen(args->argv[i]) == 0)
			args_del(args, i, 1);
		else
			i++;
	}
}

/* unquotes each argument in args
 */
void args_unquote(args_t *args)
{
	int i;

	for(i=0;i<args->argc;i++)
		unquote(args->argv[i]);
}

/* inserts STR into ARGS at index INDEX
 */
void args_insert_string(args_t *args, unsigned int index, const char *str)
{
	args_t *a;

	if(!args || !str)
		return;

	if(index > args->argc)
		return;

	a = args_create();
	args_init2(a, args->argc, args->argv, index);
	args_del(args, index, args->argc - index);
	args_push_back(args, str);
	args_add_args(args, a);
	free(a);
}

/* inserts INSARGS between FIRST and LAST (and ALWAYS?) into ARGS at index INDEX
 */
void args_insert_args(args_t *args, unsigned int index, const args_t *insargs,
					  unsigned int first, unsigned int last)
{
	args_t *a;

	if(!args || !insargs)
		return;

	if(index > args->argc)
		return;

	a = args_create();
	args_init2(a, args->argc, args->argv, index);
	args_del(args, index, args->argc - index);
	args_add_args3(args, insargs, first, last);
	args_add_args(args, a);
	free(a);
}

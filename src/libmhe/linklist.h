/* $Id: linklist.h,v 1.6 2001/05/12 18:43:01 mhe Exp $
 *
 * linklist.h -- generic linked list
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _linklist_h_included
#define _linklist_h_included

typedef int (*listfunc)(void *);
typedef void *(*listclonefunc)(void *);

/* should return < 0 if a < b, 0 if a == b, > 0 if a > b */
typedef int (*listsortfunc)(const void *a, const void *b);

/* should return 0 (zero) if ITEM matches ARG */
typedef int (*listsearchfunc)(const void *item, const void *arg);

typedef struct listitem listitem;
struct listitem {
	void *data;
	listitem *next, *prev;
};

typedef struct list list;
struct list {
	listitem *first, *last;
	listfunc freefunc;
	int numitem;
};

list *list_new(listfunc freefunc);
void list_free(list *lp);
void list_clear(list *lp);
void list_delitem(list *lp, listitem *lip);
void list_removeitem(list *lp, listitem *lip);
void list_additem(list *lp, void *data);
int list_numitem(list *lp);
listitem *list_search(list *lp, listsearchfunc cmpfunc, const void *arg);
void list_sort(list *lp, listsortfunc cmp, int reverse);
list *list_clone(list *lp, listclonefunc clonefunc);
int list_equal(list *a, list *b, listsortfunc cmpfunc);

#endif

/* linklist.h -- generic linked list
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

/* linklist.c -- generic linked list
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

#include "linklist.h"
#include "xmalloc.h"

static listitem *new_item(void)
{
	return (listitem *)xmalloc(sizeof(listitem));
}

list *list_new(listfunc freefunc)
{
	list *lp;

	lp = (list *)xmalloc(sizeof(list));
	lp->freefunc = freefunc;
	return lp;
}

void list_free(list *lp)
{
	if(!lp)
		return;
	while(lp->first)
		list_delitem(lp, lp->first);
	xfree(lp);
}

void list_clear(list *lp)
{
	if(!lp)
		return;
	while(lp->first)
		list_delitem(lp, lp->first);
}

listitem *list_search(list *lp, listsearchfunc cmpfunc, const void *arg)
{
	listitem *lip;

	if(!lp)
		return 0;

	lip = lp->first;
	while(lip) {
		if(cmpfunc(lip->data, arg) == 0)
			return lip;
		lip = lip->next;
	}
	return 0;
}

void list_delitem(list *lp, listitem *lip)
{
	if(!lp || !lp->first)
		return;

	if(lip->prev)
		lip->prev->next = lip->next;
	else
		lp->first = lip->next;
	if(lip->next)
		lip->next->prev = lip->prev;
	else
		lp->last = lip->prev;
	if(lp->freefunc)
		lp->freefunc(lip->data);
	lp->numitem--;
}

void list_additem(list *lp, void *data)
{
	listitem *lip = new_item();

	if(!lp)
		return;

	lip->data = data;
	lip->next = 0;

	if(lp->first == 0) {
		lp->last = lp->first = lip;
	} else {
		lip->prev = lp->last;
		lip->prev->next = lip;
		lp->last = lip;
	}
	lp->numitem++;
}

int list_numitem(list *lp)
{
	if(!lp)
		return 0;
	return lp->numitem;
}

static void list_swap(list *lp, listitem *a, listitem *b)
{
	void *tmp;

	if(!a || !b || a->data == b->data)
		return;

	tmp = a->data;
	a->data = b->data;
	b->data = tmp;
}

/* simple bubble-sort */
void list_sort(list *lp, listsortfunc cmp, int reverse)
{
	listitem *li, *last = 0;
	bool swapped = true;
	int c;

	while(swapped) {
		swapped = false;
		for(li=lp->first; li && li->next; li=li->next) {
			if(li->next == last) {
				last = li;
				break;
			}
			c = cmp(li->data, li->next->data);
			if(reverse ? c < 0  : c > 0) {
				list_swap(lp, li, li->next);
				swapped = true;
			}
		}
	}
}

void list_removeitem(list *lp, listitem *lip)
{
	if(!lp || !lp->first)
		return;

	if(lip->prev)
		lip->prev->next = lip->next;
	else
		lp->first = lip->next;
	if(lip->next)
		lip->next->prev = lip->prev;
	else
		lp->last = lip->prev;
	lp->numitem--;
}

list *list_clone(list *lp, listclonefunc clonefunc)
{
	list *cloned;
	listitem *li;

	if(!lp)
		return 0;
	cloned = list_new(lp->freefunc);

	for(li=lp->first; li; li=li->next)
		list_additem(cloned, clonefunc(li->data));

	return cloned;
}

int list_equal(list *a, list *b, listsortfunc cmpfunc)
{
	listitem *ai, *bi;

	if(a == b)
		return 1;

	if(list_numitem(a) != list_numitem(b) || a == 0 || b == 0)
		return 0;

	for(ai = a->first, bi = b->first; ai && bi; ai = ai->next, bi = bi->next) {
		if(cmpfunc(ai->data, bi->data) != 0)
			return 0;
	}
	return 1;
}

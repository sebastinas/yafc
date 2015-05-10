/*
 * linklist.c -- generic linked list
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 * Copyright (C) 2015, Sebastian RAmacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"

#include "linklist.h"
#include "xmalloc.h"

static listitem *new_item(void)
{
  return xmalloc(sizeof(listitem));
}

list *list_new(listfunc freefunc)
{
  list* lp = xmalloc(sizeof(list));
  lp->freefunc = freefunc;
  lp->first = NULL;
  lp->last = NULL;
  return lp;
}

void list_free(list *lp)
{
  list_clear(lp);
  free(lp);
}

void list_clear(list *lp)
{
  if (!lp)
    return;

  while (lp->first)
    list_delitem(lp, lp->first);
}

listitem *list_search(list *lp, listsearchfunc cmpfunc, const void *arg)
{
  if(!lp)
    return NULL;

  listitem* lip = lp->first;
  while (lip)
  {
    if (cmpfunc(lip->data, arg) == 0)
      return lip;
    lip = lip->next;
  }
  return NULL;
}

void list_delitem(list *lp, listitem *lip)
{
  if (!lp || !lp->first)
    return;

  if (lp->freefunc)
    lp->freefunc(lip->data);

  list_removeitem(lp, lip);
  free(lip);
}

void list_additem(list *lp, void *data)
{
  if (!lp)
    return;

  listitem* lip = new_item();
  lip->data = data;
  lip->next = NULL;
  lip->prev = NULL;

  if (!lp->first)
    lp->last = lp->first = lip;
  else
  {
    listitem* current_last = lp->last;
    lip->prev = current_last;
    current_last->next = lip;
    lp->last = lip;
  }
  lp->numitem++;
}

size_t list_numitem(list *lp)
{
  if (!lp)
    return 0;

  return lp->numitem;
}

static void list_swap(list *lp, listitem *a, listitem *b)
{
  if (!a || !b || a->data == b->data)
    return;

  void* tmp = a->data;
  a->data = b->data;
  b->data = tmp;
}

/* simple bubble-sort */
void list_sort(list *lp, listsortfunc cmp, bool reverse)
{
  listitem *last = NULL;
  bool swapped = true;

  while (swapped)
  {
    swapped = false;
    for (listitem* li = lp->first; li && li->next; li = li->next)
    {
      if (li->next == last)
      {
        last = li;
        break;
      }
      const int c = cmp(li->data, li->next->data);
      if (reverse ? c < 0 : c > 0)
      {
        list_swap(lp, li, li->next);
        swapped = true;
      }
    }
  }
}

void list_removeitem(list *lp, listitem *lip)
{
  if (!lp || !lp->first)
    return;

  if (lip->prev)
    lip->prev->next = lip->next;
  else
    lp->first = lip->next;
  if (lip->next)
    lip->next->prev = lip->prev;
  else
    lp->last = lip->prev;
  lp->numitem--;
}

list *list_clone(list *lp, listclonefunc clonefunc)
{
  if (!lp || !clonefunc)
    return NULL;

  list* cloned = list_new(lp->freefunc);
  for (listitem* li = lp->first; li; li = li->next)
    list_additem(cloned, clonefunc(li->data));

  return cloned;
}

int list_equal(list *a, list *b, listsortfunc cmpfunc)
{
  if (a == b)
    return 1;

  if (list_numitem(a) != list_numitem(b) || a == 0 || b == 0)
    return 0;

  for (listitem* ai = a->first, *bi = b->first; ai && bi; ai = ai->next, bi = bi->next)
  {
    if (cmpfunc(ai->data, bi->data) != 0)
      return 0;
  }
  return 1;
}

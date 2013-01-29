/*
 * lscolors.c -- parsing of env variable LS_COLORS
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 * Copyright (C) 2012, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "rfile.h"
#include "xmalloc.h"
#include "strq.h"
#include "lscolors.h"

typedef struct {
  char *msk;
  char *clr;
} lscolor;

static bool colors_initialized = false;
static int number_of_colors = 0;

/* these are the default colors */
static char *diclr = NULL, *lnclr = NULL, *ficlr = NULL, *exclr = NULL;
static char *bdclr = NULL, *cdclr = NULL, *piclr = NULL, *soclr = NULL;
static char *lc = NULL, *rc = NULL, *ec = NULL;

static lscolor *clrs = NULL;

char *endcolor(void)
{
  return ec;
}

void init_colors(void)
{
  int i = 0;
  char *p, *e, *org;
  unsigned int n;

  if(colors_initialized)
    return;

  p = getenv("LS_COLORS");
  if(!p)
    p = getenv("LS_COLOURS");

  if(p && *p) {
    org = p = xstrdup(p);
    n = strqnchr(p, ':')+2;
    clrs = (lscolor *)xmalloc(n * sizeof(lscolor));
    e = strqsep(&p, ':');
    while(e && *e) {
      char* msk = strqsep(&e, '=');
      char* clr = xstrdup(strqsep(&e, ':'));
      e = strqsep(&p, ':');

      if(strcmp(msk, "di")==0) /* directory */
        diclr = clr;
      else if(strcmp(msk, "ln")==0) /* link */
        lnclr = clr;
      else if(strcmp(msk, "fi")==0) /* normal file */
        ficlr = clr;
      else if(strcmp(msk, "ex")==0) /* executable */
        exclr = clr;
      else if(strcmp(msk, "bd")==0) /* block device */
        bdclr = clr;
      else if(strcmp(msk, "cd")==0) /* character device */
        cdclr = clr;
      else if(strcmp(msk, "pi")==0) /* pipe */
        piclr = clr;
      else if(strcmp(msk, "so")==0) /* socket */
        soclr = clr;
      else if(strcmp(msk, "lc") == 0) /* left code */
        lc = clr;
      else if(strcmp(msk, "rc") == 0) /* right code */
        rc = clr;
      else if(strcmp(msk, "ec") == 0) /* end code (lc+fi+rc) */
        ec = clr;
      else
      {
        clrs[i].msk = xstrdup(msk);
        clrs[i].clr = clr;
        i++;
      }
    }
    number_of_colors = i;
    free(org);
    if(!ec && lc && ficlr && rc) {
      const size_t len = strlen(lc)+strlen(ficlr)+strlen(rc)+1;
      ec = xmalloc(len);
      snprintf(ec, len, "%s%s%s", lc, ficlr, rc);
    }
  }

  if (!diclr)
    diclr = xstrdup("01;34");
  if (!lnclr)
    lnclr = xstrdup("01;36");
  if (!ficlr)
    ficlr = xstrdup("00");
  if (!exclr)
    exclr = xstrdup("01;32");
  if (!bdclr)
    bdclr = xstrdup("40;01;33");
  if (!cdclr)
    cdclr = xstrdup("40;01;33");
  if (!piclr)
    piclr = xstrdup("40;33");
  if (!soclr)
    soclr = xstrdup("01;35");
  if (!lc)
    lc = xstrdup("\x1B[");
  if (!rc)
    rc = xstrdup("m");
  if (!ec)
    ec = xstrdup("\x1B[0m");

  colors_initialized = true;
}

void free_colors(void)
{
  if (!colors_initialized)
    return;

  for (unsigned int i = 0; i != number_of_colors; ++i)
  {
    free(clrs[i].msk);
    free(clrs[i].clr);
  }
  free(clrs);
  free(diclr);
  free(lnclr);
  free(ficlr);
  free(exclr);
  free(bdclr);
  free(cdclr);
  free(piclr);
  free(soclr);
  free(lc);
  free(rc);
  free(ec);

  clrs = NULL;
  diclr = lnclr = ficlr = exclr = bdclr = cdclr = piclr = soclr = lc =
    rc = ec = NULL;
  colors_initialized = false;
}

void rfile_parse_colors(rfile *f)
{
  char *e, *q;
  int i;

  /* colors should already been initialized by init_colors */

  e = 0;
  if(risdir(f))
    e = diclr;
  else if(rislink(f))
    e = lnclr;
  else if(rischardev(f))
    e = cdclr;
  else if(risblockdev(f))
    e = bdclr;
  else if(rispipe(f))
    e = piclr;
  else if(rissock(f))
    e = soclr;

  /* do this before checking for executable, because
   * on [v]fat filesystems, all files are 'executable'
   */
  for(i=0;i<number_of_colors && !e;i++) {
    if(fnmatch(clrs[i].msk, base_name_ptr(f->path), 0) == 0)
      e = clrs[i].clr;
  }

  /* a file is considered executable if there is
   * an 'x' anywhere in its permission string */
  if(!e && risexec(f))
    e = exclr;

  q = e ? e : ficlr;
  f->color = (char *)xmalloc(strlen(lc) + strlen(q) + strlen(rc) + 1);
  sprintf(f->color, "%s%s%s", lc, q, rc);
}

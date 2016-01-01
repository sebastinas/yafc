/*
 * set.c -- sets and shows variables from within Yafc
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
#include "ftp.h"
#include "gvars.h"
#include "strq.h"
#include "set.h"

static void set_autologin(void *val)
{
	if(val)
		gvAutologin = *(bool *)val;

	if(gvAutologin)
		puts(_("autologin is on"));
	else
		puts(_("autologin is off"));
}

static void set_verbose(void *val)
{
	bool b;

	if(val) {
		b = *(bool *)val;
		ftp_set_verbosity(b ? vbCommand : vbError);
	}

	if(ftp->verbosity == vbCommand) {
		puts(_("verbose is on"));
		gvVerbose = true;
	} else {
		puts(_("verbose is off"));
		gvVerbose = false;
	}
}

static void set_pasvmode(void *val)
{
  if (val)
  {
    gvPasvmode = *(bool *)val;
    if (ftp && ftp->url)
      ftp->url->pasvmode = gvPasvmode;
  }

  const bool b = (ftp && ftp->url && ftp->url->pasvmode != -1) ?
    ftp->url->pasvmode : gvPasvmode;
  if (b)
    puts(_("passive mode is on"));
  else
    puts(_("passive mode is off"));
}

static void set_debug(void *val)
{
	bool b;

	if(val) {
		b = *(bool *)val;
		ftp_set_verbosity(b ? vbDebug : vbError);
	}

	if(ftp->verbosity == vbDebug) {
		puts(_("debug is on"));
		gvDebug = true;
	} else {
		puts(_("debug is off"));
		gvDebug = false;
	}
}

static void set_type(void *val)
{
	if(val) {
		if(strcasecmp((char *)val, "binary") == 0
		   || strcasecmp((char *)val, "I") == 0)
			gvDefaultType = tmBinary;
		else if(strcasecmp((char *)val, "ascii") == 0
				|| strcasecmp((char *)val, "A") == 0)
			gvDefaultType = tmAscii;
		else {
			printf(_("Unknown type '%s'? Use 'ascii' or 'binary'\n"),
				   (char *)val);
			return;
		}
	}
	printf(_("default type is '%s'\n"),
		   gvDefaultType==tmBinary ? "binary" : "ascii");
}

/* changes password used with anonymous connections */
static void set_anonpass(void *val)
{
	if(val) {
		free(gvAnonPasswd);
		gvAnonPasswd = xstrdup((char *)val);
	}
	printf(_("anonymous password is '%s'\n"), gvAnonPasswd);
}

struct _setvar setvariables[] = {
	{"autologin", ARG_BOOL, set_autologin},
	{"debug", ARG_BOOL, set_debug},
	{"verbose", ARG_BOOL, set_verbose},
	{"passive_mode", ARG_BOOL, set_pasvmode},
	{"type", ARG_STR, set_type},
	{"anonpass", ARG_STR, set_anonpass},
	{NULL, 0, NULL}
};

static struct _setvar *get_variable(char *varname)
{
	int i;
	struct _setvar *r = 0;

	for(i=0;setvariables[i].name;i++) {
		/* compare only strlen(varname) chars, allowing variables
		 * to be shortened, as long as they're not ambiguous
		 */
		if(strncmp(setvariables[i].name, varname, strlen(varname)) == 0) {
			/* is this an exact match? */
			if(strlen(varname) == strlen(setvariables[i].name))
				return &setvariables[i];

			if(r)
				r = (struct _setvar *)-1;
			else
				r = &setvariables[i];
		}
	}
	return r;
}

void cmd_set(int argc, char **argv)
{
	struct _setvar *sp;
	int n;
	void *vp = 0;

	if(argc < 2) {
		for(n = 0; setvariables[n].name; n++)
			setvariables[n].setfunc(0);
	} else {
		sp = get_variable(argv[1]);
		if(!sp) {
			fprintf(stderr, _("No such variable '%s'\n"), argv[1]);
			return;
		} else if(sp == (struct _setvar *)-1) {
			fprintf(stderr, _("Ambiguous variable '%s'\n"), argv[1]);
			return;
		}

		if(argc == 2)
			sp->setfunc(0);
		else {
			if(sp->argtype == ARG_BOOL) {
				if((n = str2bool(argv[2])) == -1) {
					printf(_("Expected boolean value, but got '%s'\n"),
						   argv[2]);
					return;
				}
				vp = &n;
			} else if(sp->argtype == ARG_INT) {
				n = atoi(argv[2]);
				vp = &n;
			} else if(sp->argtype == ARG_STR) {
				vp = argv[2];
			}
			sp->setfunc(vp);
		}
	}
}

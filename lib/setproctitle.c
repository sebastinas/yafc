/* modified for use in yafc by mhe@stacken.kth.se
 * last changed: 1999-04-24
 */

/*
 * setproctitle implementation for linux.
 * Stolen from sendmail 8.7.4 and bashed around by David A. Holland
 */

/*
 * Copyright (c) 1983, 1995 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From: @(#)conf.c	8.243 (Berkeley) 11/20/95
 */

/*char setproctitle_rcsid[] =
  "$Id: setproctitle.c,v 1.1 2000/09/14 14:06:19 mhe Exp $";*/

#include "syshdr.h"

/*
**  SETPROCTITLE -- set process title for ps
**
**	Parameters:
**		fmt -- a printf style format string.
**		a, b, c -- possible parameters to fmt.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Clobbers argv of our main procedure so ps(1) will
**		display the title.
*/


/*
**  Pointers for setproctitle.
**	This allows "ps" listings to give more useful information.
*/

#ifdef linux
static char **Argv = NULL;		/* pointer to argument vector */
static char *LastArgv = NULL;		/* end of argv */
static char Argv0[128];			/* program name */
#endif

void initsetproctitle(int argc, char **argv, char **envp)
{
#ifdef linux
	register int i;
	char *tmp;

	/*
	 **  Move the environment so setproctitle can use the space at
	 **  the top of memory.
	 */

	for (i = 0; envp[i] != NULL; i++)
		continue;
	__environ = (char **) malloc(sizeof (char *) * (i + 1));
	for (i = 0; envp[i] != NULL; i++)
		__environ[i] = strdup(envp[i]);
	__environ[i] = NULL;

	/*
	 **  Save start and extent of argv for setproctitle.
	 */

	Argv = argv;
	if (i > 0)
		LastArgv = envp[i - 1] + strlen(envp[i - 1]);
	else
		LastArgv = argv[argc - 1] + strlen(argv[argc - 1]);

	tmp = strrchr(argv[0], '/');
	if (!tmp) tmp = argv[0];
	else tmp++;
	strncpy(Argv0, tmp, sizeof(Argv0));
	Argv0[sizeof(Argv0)] = 0;
#endif
}

void setproctitle(const char *fmt, ...)
{
#ifdef linux
	register char *p;
	register int i;
	static char buf[2048];
	va_list ap;

	p = buf;

	/* print progname: heading for grep */
	/* This can't overflow buf due to the relative size of Argv0. */
	(void) strcpy(p, Argv0);
	(void) strcat(p, ": ");
	p += strlen(p);

	/* print the argument string */
	va_start(ap, fmt);
	(void) vsnprintf(p, sizeof(buf) - (p - buf), fmt, ap);
	va_end(ap);

	i = strlen(buf);

	if (i > LastArgv - Argv[0] - 2)
	{
		i = LastArgv - Argv[0] - 2;
		buf[i] = '\0';
	}
	(void) strcpy(Argv[0], buf);
	p = &Argv[0][i];
	while (p < LastArgv)
		*p++ = ' ';
	Argv[1] = NULL;
#endif
}

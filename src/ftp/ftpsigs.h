/* ftpsigs.h -- handles signals
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

#ifndef _ftpsigs_h_
#define _ftpsigs_h_

#include "syshdr.h"

void ftp_set_signal(int signum, sighandler_t handler);

void ftp_initsigs(void);
void ftp_sigint_close_reset(void);
void ftp_set_sigint(void);
void ftp_unset_sigint(void);
unsigned int ftp_sigints(void);

int ftp_longjmp(
#ifdef HAVE_POSIX_SIGSETJMP
				sigjmp_buf restart_jmp
#else
				jmp_buf restart_jmp
#endif
				, int arg);

RETSIGTYPE sigint_jmp_handler(int signum);
RETSIGTYPE sigint_close_handler(int signum);
RETSIGTYPE sigint_abort_handler(int signum);

void ftp_set_close_handler(void);
void ftp_set_abort_handler(void);

#endif

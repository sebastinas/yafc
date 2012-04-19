/* $Id: get.h,v 1.5 2002/12/02 12:21:19 mhe Exp $
 *
 * get.h -- get file(s) from remote
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _get_h_included
#define _get_h_included

#define GET_INTERACTIVE 1
#define GET_APPEND (1 << 1)
#define GET_PRESERVE (1 << 2)
#define GET_PARENTS (1 << 3)
#define GET_RECURSIVE (1 << 4)
#define GET_VERBOSE (1 << 5)
#define GET_FORCE (1 << 6)
#define GET_NO_DEREFERENCE (1 << 7)
#define GET_NEWER (1 << 8)
#define GET_DELETE_AFTER (1 << 9)
#define GET_UNIQUE (1 << 10)
#define GET_RESUME (1 << 11)
#define GET_TAGGED (1 << 12)
#define GET_NOHUP (1 << 13)
#define GET_SKIP_EXISTING (1 << 14)
#define GET_BACKGROUND (1 << 15)
#define GET_ASCII (1 << 16)
#define GET_BINARY (1 << 17)
#define GET_CHMOD (1 << 18)
#define GET_CHGRP (1 << 19)
#define GET_OUTPUT_FILE (1 << 20)  /* --output=FILE (else --output=DIR) */
#define GET_SKIP_EMPTY (1 << 21)

#endif

/* put.h -- 
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

#ifndef _put_h_included
#define _put_h_included

#define PUT_INTERACTIVE 1
#define PUT_APPEND 2
#define PUT_PRESERVE 4
#define PUT_PARENTS 8
#define PUT_RECURSIVE 16
#define PUT_VERBOSE 32
#define PUT_FORCE 64
#define PUT_OUTPUT_FILE 128  /* --output=FILE (else --output=DIR) */
#define PUT_UNIQUE 256
#define PUT_DELETE_AFTER 512
#define PUT_SKIP_EXISTING 1024
#define PUT_NOHUP 2048
#define PUT_RESUME 4096
#define PUT_NEWER 8192
#define PUT_TAGGED 16384
#define PUT_ASCII 32768
#define PUT_BINARY 65536

#endif

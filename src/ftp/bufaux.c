/* Modified for use in Yafc by Martin Hedenfalk <mhe@stacken.kth.se>
 */

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Auxiliary functions for storing and retrieving various data types to/from
 * Buffers.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * SSH2 packet format added by Markus Friedl
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "syshdr.h"
#include "ftp.h"
/*#include "includes.h"*/
/*RCSID("$OpenBSD: bufaux.c,v 1.17 2001/01/21 19:05:45 markus Exp $");*/

/*#include <openssl/bn.h>*/
#include "bufaux.h"
/*#include "xmalloc.h"*/
#include "getput.h"
/*#include "log.h"*/

/* bignum functions removed ... */

/*
 * Returns an integer from the buffer (4 bytes, msb first).
 */
u_int
buffer_get_int(Buffer *buffer)
{
	u_char buf[4];
	buffer_get(buffer, (char *) buf, 4);
	return GET_32BIT(buf);
}

u_int64_t
buffer_get_int64(Buffer *buffer)
{
	u_char buf[8];
	buffer_get(buffer, (char *) buf, 8);
	return GET_64BIT(buf);
}

/*
 * Stores an integer in the buffer in 4 bytes, msb first.
 */
void
buffer_put_int(Buffer *buffer, u_int value)
{
	char buf[4];
	PUT_32BIT(buf, value);
	buffer_append(buffer, buf, 4);
}

void
buffer_put_int64(Buffer *buffer, u_int64_t value)
{
	char buf[8];
	PUT_64BIT(buf, value);
	buffer_append(buffer, buf, 8);
}

/*
 * Returns an arbitrary binary string from the buffer.  The string cannot
 * be longer than 256k.  The returned value points to memory allocated
 * with xmalloc; it is the responsibility of the calling function to free
 * the data.  If length_ptr is non-NULL, the length of the returned data
 * will be stored there.  A null character will be automatically appended
 * to the returned string, and is not counted in length.
 */
char *
buffer_get_string(Buffer *buffer, u_int *length_ptr)
{
	u_int len;
	char *value;
	/* Get the length. */
	len = buffer_get_int(buffer);
	if (len > 256 * 1024) {
		ftp_err("Received packet with bad string length %d", len);
		return 0;
	}
	/* Allocate space for the string.  Add one byte for a null character. */
	value = xmalloc(len + 1);
	/* Get the string. */
	buffer_get(buffer, value, len);
	/* Append a null character to make processing easier. */
	value[len] = 0;
	/* Optionally return the length of the string. */
	if (length_ptr)
		*length_ptr = len;
	return value;
}

/*
 * Stores and arbitrary binary string in the buffer.
 */
void
buffer_put_string(Buffer *buffer, const void *buf, u_int len)
{
	buffer_put_int(buffer, len);
	buffer_append(buffer, buf, len);
}
void
buffer_put_cstring(Buffer *buffer, const char *s)
{
	buffer_put_string(buffer, s, strlen(s));
}

/*
 * Returns a character from the buffer (0 - 255).
 */
int
buffer_get_char(Buffer *buffer)
{
	char ch;
	buffer_get(buffer, &ch, 1);
	return (u_char) ch;
}

/*
 * Stores a character in the buffer.
 */
void
buffer_put_char(Buffer *buffer, int value)
{
	char ch = value;
	buffer_append(buffer, &ch, 1);
}

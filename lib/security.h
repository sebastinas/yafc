/* modified by Martin Hedenfalk <mhe@home.se> 19 aug 2000
 */

/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


#ifndef __security_h__
#define __security_h__

struct buffer {
    void *data;
    size_t size;
    size_t index;
    int eof_flag;
};

enum protection_level { 
    prot_clear, 
    prot_safe, 
    prot_confidential, 
    prot_private 
};

struct sec_client_mech {
    char *name;
    size_t size;
    int (*init)(void *);
    int (*auth)(void *, const char*);
    void (*end)(void *);
    int (*check_prot)(void *, int);
    int (*overhead)(void *, int, int);
    int (*encode)(void *, const void*, int, int, void**);
    int (*decode)(void *, void*, int, int);
};

#define AUTH_OK		0
#define AUTH_CONTINUE	1
#define AUTH_ERROR	2

extern struct sec_client_mech gss_client_mech;
extern struct sec_client_mech ssl_client_mech;

/*extern int sec_complete;*/

/* ---- */


int sec_fflush (FILE *);
int sec_fprintf (FILE *, const char *, ...);
int sec_getc (FILE *);
int sec_putc (int, FILE *);
int sec_read (int, void *, int);
int sec_read_msg (char *, int);
int sec_vfprintf (FILE *, const char *, va_list);
int sec_fprintf2(FILE *f, const char *fmt, ...);
int sec_vfprintf2(FILE *, const char *, va_list);
int sec_write (int, const char *, int);

void sec_end (void);
int sec_login(const char *host, const char *mech_to_try);
void sec_prot (int, char **);
int sec_request_prot (char *);
void sec_set_protection_level (void);
void sec_status (void);

enum protection_level set_command_prot(enum protection_level);

const char *level_to_name(enum protection_level level);
enum protection_level name_to_level(const char *name);

#endif /* __security_h__ */  

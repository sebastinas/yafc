/* Modified for use in Yafc by Martin Hedenfalk <mhe@home.se>
 * last changed: Dec 2002
 */

/*
 * Copyright (c) 1995-1999 Kungliga Tekniska Högskolan
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

/*#include <des.h>*/
#include <krb.h>

#include "syshdr.h"
#include "ftp.h"
#include "base64.h"
#include "commands.h"
#include "args.h"

#ifndef KRB_TICKET_GRANTING_TICKET
# define KRB_TICKET_GRANTING_TICKET "krbtgt"
#endif


void cmd_kauth(int argc, char **argv)
{
    int ret;
    char *pwbuf;
    des_cblock key;
    des_key_schedule schedule;
    KTEXT_ST tkt, tktcopy;
    char *name;
    char *p;
    char passwd[100];
    int tmp;
    int save;

	OPT_HELP("Authenticate to Kerberos.  Usage:\n"
			 "  kauth [options] [principal name]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	maxargs(1);
	
	need_connected();
	need_loggedin();

    if(argc == 2)
		name = argv[1];
    else
		name = ftp->url->username;

    save = set_command_prot(prot_private);

	ftp_set_tmp_verbosity(vbError);
    ret = ftp_cmd("SITE KAUTH %s", name);
    if(ftp->code != ctContinue) {
		set_command_prot(save);
		return;
	}
    p = strstr(ftp->reply, "T=");
    if(!p) {
		ftp_err(_("Bad reply from server\n"));
		set_command_prot(save);
		return;
    }
    p += 2;
    tmp = base64_decode(p, &tkt.dat);
    if(tmp < 0) {
		ftp_err(_("Failed to decode base64 in reply\n"));
		set_command_prot(save);
		return;
    }
    tkt.length = tmp;
    tktcopy.length = tkt.length;
    
    p = strstr(ftp->reply, "P=");
    if(!p) {
		ftp_err(_("Bad reply from server\n"));
		set_command_prot(save);
		return;
    }
    name = p + 2;
    for(; *p && *p != ' ' && *p != '\r' && *p != '\n'; p++);
    *p = 0;

    asprintf(&pwbuf, _("Password for %s:"), name);
    if (des_read_pw_string (passwd, sizeof(passwd)-1, pwbuf, 0))
        *passwd = '\0';
	free(pwbuf);
    des_string_to_key(passwd, &key);

    des_key_sched(&key, schedule);
    
    des_pcbc_encrypt((des_cblock*)tkt.dat, (des_cblock*)tktcopy.dat,
					 tkt.length,
					 schedule, &key, DES_DECRYPT);
#ifdef HAVE_AFS_STRING_TO_KEY
    if (strcmp ((char*)tktcopy.dat + 8,
				KRB_TICKET_GRANTING_TICKET) != 0)
	{
        afs_string_to_key (passwd,
						   krb_realmofhost(ftp->host->ohostname), &key);
		des_key_sched (&key, schedule);
		des_pcbc_encrypt((des_cblock*)tkt.dat, (des_cblock*)tktcopy.dat,
						 tkt.length,
						 schedule, &key, DES_DECRYPT);
    }
#endif
    memset(key, 0, sizeof(key));
    memset(schedule, 0, sizeof(schedule));
    memset(passwd, 0, sizeof(passwd));
    if(base64_encode(tktcopy.dat, tktcopy.length, &p) < 0) {
		ftp_err(_("Out of memory base64-encoding\n"));
		set_command_prot(save);
		return;
    }
    memset (tktcopy.dat, 0, tktcopy.length);
    ret = ftp_cmd("SITE KAUTH %s %s", name, p);
    free(p);
	set_command_prot(save);
}

void cmd_klist(int argc, char **argv)
{
	OPT_HELP("List Kerberos tickets.  Usage:\n"
			 "  klist [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	maxargs(0);
	need_connected();
	need_loggedin();

	ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("SITE KLIST");
}

void cmd_kdestroy(int argc, char **argv)
{
	OPT_HELP("Destroy Kerberos tickets.  Usage:\n"
			 "  kdestroy [options]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	maxargs(0);
	need_connected();
	need_loggedin();
	ftp_set_tmp_verbosity(vbCommand);
	ftp_cmd("SITE KDESTROY");
}

void cmd_krbtkfile(int argc, char **argv)
{
	OPT_HELP("Set file used for Kerberos tickets.  Usage:\n"
			 "  krbtkfile [options] file\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(1);
	maxargs(1);
	need_connected();
	need_loggedin();
	ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("SITE KRBTKFILE %s", argv[1]);
}

void cmd_afslog(int argc, char **argv)
{
	OPT_HELP("Obtain AFS tokens for specified cell.  Usage:\n"
			 "  afslog [options] [cell]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	maxargs(2);
	need_connected();
	need_loggedin();
	ftp_set_tmp_verbosity(vbCommand);
    if(argc == 2)
		ftp_cmd("SITE AFSLOG %s", argv[1]);
    else
		ftp_cmd("SITE AFSLOG");
}

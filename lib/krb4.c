/* modified by Martin Hedenfalk <mhe@stacken.kth.se> 19 aug 2000
 */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#ifdef FTP_SERVER
#include "ftpd_locl.h"
#else
# include "syshdr.h"
# include "ftp.h"
/*# include <syslog.h>*/
# include "base64.h"
extern int krb_get_our_ip_for_realm(
    const char *realm, struct in_addr *ip_addr);
#endif
/*#include <krb.h>*/

/*RCSID("$Id: krb4.c,v 1.1.1.1 2000/09/14 14:06:18 mhe Exp $");*/

#ifdef FTP_SERVER
#define LOCAL_ADDR ctrl_addr
#define REMOTE_ADDR his_addr
#else
#define LOCAL_ADDR (&ftp->ctrl->local_addr)
#define REMOTE_ADDR (&ftp->ctrl->remote_addr)
#define myctladdr (&ftp->ctrl->local_addr)
#define hisctladdr (&ftp->ctrl->remote_addr)
#endif

/*extern struct sockaddr *LOCAL_ADDR, *REMOTE_ADDR;*/

struct krb4_data {
    des_cblock key;
    des_key_schedule schedule;
    char name[ANAME_SZ];
    char instance[INST_SZ];
    char realm[REALM_SZ];
};

static int
krb4_check_prot(void *app_data, int level)
{
    if(level == prot_confidential)
	return -1;
    return 0;
}

static int
krb4_decode(void *app_data, void *buf, int len, int level)
{
    MSG_DAT m;
    int e;
    struct krb4_data *d = app_data;
    
    if(level == prot_safe)
	e = krb_rd_safe(buf, len, &d->key,
			(struct sockaddr_in *)REMOTE_ADDR,
			(struct sockaddr_in *)LOCAL_ADDR, &m);
    else
	e = krb_rd_priv(buf, len, d->schedule, &d->key, 
			(struct sockaddr_in *)REMOTE_ADDR,
			(struct sockaddr_in *)LOCAL_ADDR, &m);
    if(e){
	ftp_err("krb4_decode: %s", krb_get_err_text(e));
	return -1;
    }
    memmove(buf, m.app_data, m.app_length);
    return m.app_length;
}

static int
krb4_overhead(void *app_data, int level, int len)
{
    return 31;
}

static int
krb4_encode(void *app_data, void *from, int length, int level, void **to)
{
    struct krb4_data *d = app_data;
    *to = malloc(length + 31);
    if(level == prot_safe)
	return krb_mk_safe(from, *to, length, &d->key, 
			   (struct sockaddr_in *)LOCAL_ADDR,
			   (struct sockaddr_in *)REMOTE_ADDR);
    else if(level == prot_private)
	return krb_mk_priv(from, *to, length, d->schedule, &d->key, 
			   (struct sockaddr_in *)LOCAL_ADDR,
			   (struct sockaddr_in *)REMOTE_ADDR);
    else
	return -1;
}

#ifdef FTP_SERVER

/* snip */

#else /* FTP_SERVER */

static int
mk_auth(struct krb4_data *d, KTEXT adat, 
	char *service, char *host, int checksum)
{
    int ret;
    CREDENTIALS cred;
    char sname[SNAME_SZ], inst[INST_SZ], realm[REALM_SZ];

    strlcpy(sname, service, sizeof(sname));
    strlcpy(inst, krb_get_phost(host), sizeof(inst));
    strlcpy(realm, krb_realmofhost(host), sizeof(realm));
    ret = krb_mk_req(adat, sname, inst, realm, checksum);
    if(ret)
	return ret;
    strlcpy(sname, service, sizeof(sname));
    strlcpy(inst, krb_get_phost(host), sizeof(inst));
    strlcpy(realm, krb_realmofhost(host), sizeof(realm));
    ret = krb_get_cred(sname, inst, realm, &cred);
    memmove(&d->key, &cred.session, sizeof(des_cblock));
    des_key_sched(&d->key, d->schedule);
    memset(&cred, 0, sizeof(cred));
    return ret;
}

static int
krb4_auth(void *app_data, char *host)
{
    int ret;
    char *p;
    int len;
    KTEXT_ST adat;
    MSG_DAT msg_data;
    int checksum;
    u_int32_t cs;
    struct krb4_data *d = app_data;
#ifdef HAVE_KRB_GET_OUR_IP_FOR_REALM
    struct sockaddr_in *localaddr  = (struct sockaddr_in *)LOCAL_ADDR;
/*  struct sockaddr_in *remoteaddr = (struct sockaddr_in *)REMOTE_ADDR;*/
#endif

    checksum = getpid();
    ret = mk_auth(d, &adat, "ftp", host, checksum);
    if(ret == KDC_PR_UNKNOWN)
	ret = mk_auth(d, &adat, "rcmd", host, checksum);
    if(ret){
	printf("%s\n", krb_get_err_text(ret));
	return AUTH_CONTINUE;
    }

#ifdef HAVE_KRB_GET_OUR_IP_FOR_REALM
    if (krb_get_config_bool("nat_in_use")) {
      struct in_addr natAddr;

      if (krb_get_our_ip_for_realm(krb_realmofhost(host),
				   &natAddr) != KSUCCESS
	  && krb_get_our_ip_for_realm(NULL, &natAddr) != KSUCCESS)
	ftp_err(_("Can't get address for realm %s\n"),
	       krb_realmofhost(host));
      else {
	if (natAddr.s_addr != localaddr->sin_addr.s_addr) {
	  ftp_err(_("Using NAT IP address (%s) for kerberos 4\n"),
		 inet_ntoa(natAddr));
	  localaddr->sin_addr = natAddr;
	  
	  /*
	   * This not the best place to do this, but it
	   * is here we know that (probably) NAT is in
	   * use!
	   */

	  if(ftp->pasvmode == false) {
		  ftp->pasvmode = true;
		  printf(_("Forced passive mode on\n"));
	  }
	}
      }
    }
#endif

/*    printf("Local address is %s\n", inet_ntoa(localaddr->sin_addr));
      printf("Remote address is %s\n", inet_ntoa(remoteaddr->sin_addr));*/

   if(base64_encode(adat.dat, adat.length, &p) < 0) {
	ftp_err(_("Out of memory base64-encoding\n"));
	return AUTH_CONTINUE;
    }
    ret = ftp_cmd("ADAT %s", p);
    free(p);

    if(ftp->code != ctComplete){
	ftp_err(_("Server didn't accept auth data\n"));
	return AUTH_ERROR;
    }

    p = strstr(ftp->reply, "ADAT=");
    if(!p){
	ftp_err(_("Remote host didn't send adat reply\n"));
	return AUTH_ERROR;
    }
    p += 5;
    len = base64_decode(p, adat.dat);
    if(len < 0){
	ftp_err(_("Failed to decode base64 from server\n"));
	return AUTH_ERROR;
    }
    adat.length = len;
    ret = krb_rd_safe(adat.dat, adat.length, &d->key, 
		      (struct sockaddr_in *)hisctladdr, 
		      (struct sockaddr_in *)myctladdr, &msg_data);
    if(ret){
	ftp_err(_("Error reading reply from server: %s\n"),
	       krb_get_err_text(ret));
	return AUTH_ERROR;
    }
    krb_get_int(msg_data.app_data, &cs, 4, 0);
    if(cs - checksum != 1){
	ftp_err(_("Bad checksum returned from server\n"));
	return AUTH_ERROR;
    }
    return AUTH_OK;
}

struct sec_client_mech krb4_client_mech = {
    "KERBEROS_V4",
    sizeof(struct krb4_data),
    NULL, /* init */
    krb4_auth,
    NULL, /* end */
    krb4_check_prot,
    krb4_overhead,
    krb4_encode,
    krb4_decode
};

#endif /* FTP_SERVER */

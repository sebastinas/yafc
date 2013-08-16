/* -*- tab-width: 8; -*-
 * Copyright (c) 1998 - 2002 Kungliga Tekniska Högskolan
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

#include "syshdr.h"
#include "ftp.h"
#include "base64.h"

#if defined(HAVE_GSSAPI_H)
# include <gssapi.h>
 /* if we have gssapi.h (not gssapi/gssapi.h) we assume we link
  * against Heimdal, which needs krb5.h to define *
  * KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN
  */
# include <krb5.h>
#elif defined(HAVE_GSSAPI_GSSAPI_H)
# include <gssapi/gssapi.h>
 /* if we have gssapi/gssapi.h it might be safe to assume we have the
  * other two that are part of MIT's krb5 as well, but this will work
  * even if they one day do away with one of those two header files.
  */
# if defined(HAVE_GSSAPI_GSSAPI_KRB5_H)
#   include <gssapi/gssapi_krb5.h>
# endif
 /* This is needed in Gentoo's Heimdal install which correctly creates
  * symlinks to match MIT's distribution locations for gssapi/ *.h
  */
# if defined(HAVE_GSSAPI_KRB5_ERR_H)
#   include <gssapi/krb5_err.h>
# endif
#else
# error "Need gssapi.h from either Heimdal or MIT krb5"
#endif




/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x04"}, corresponding to an
 * object-identifier value of {iso(1) member-body(2)
 * Unites States(840) mit(113554) infosys(1) gssapi(2)
 * generic(1) service_name(4)}.  The constant
 * GSS_C_NT_HOSTBASED_SERVICE should be initialized
 * to point to that gss_OID_desc.
 */
static gss_OID_desc gss_c_nt_hostbased_service_oid_desc =
{10, (void *)"\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x04"};


struct gss_data {
    gss_ctx_id_t context_hdl;
    char *client_name;
    gss_cred_id_t delegated_cred_handle;
};

static int
gss_init(void *app_data)
{
    struct gss_data *d = app_data;
    d->context_hdl = GSS_C_NO_CONTEXT;
    d->delegated_cred_handle = NULL;
    return 0;
}

static int
gss_check_prot(void *app_data, int level)
{
    if(level == prot_confidential)
	return -1;
    return 0;
}

static int
gss_decode(void *app_data, void *buf, int len, int level)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc input, output;
    gss_qop_t qop_state;
    int conf_state;
    struct gss_data *d = app_data;
    size_t ret_len;

    input.length = len;
    input.value = buf;
    maj_stat = gss_unwrap (&min_stat,
			   d->context_hdl,
			   &input,
			   &output,
			   &conf_state,
			   &qop_state);
    if(GSS_ERROR(maj_stat))
	return -1;
    memmove(buf, output.value, output.length);
    ret_len = output.length;
    gss_release_buffer(&min_stat, &output);
    return ret_len;
}

static int
gss_overhead(void *app_data, int level, int len)
{
    return 100; /* dunno? */
}


static int
gss_encode(void *app_data, const void *from, int length, int level, void **to)
{
    OM_uint32 min_stat;
    gss_buffer_desc input, output;
    int conf_state;
    struct gss_data *d = app_data;

    input.length = length;
    input.value = (void*) from;
    gss_wrap (&min_stat,
			 d->context_hdl,
			 level == prot_private,
			 GSS_C_QOP_DEFAULT,
			 &input,
			 &conf_state,
			 &output);
    *to = output.value;
    return output.length;
}

static void
sockaddr_to_gss_address (const struct sockaddr *sa,
			 OM_uint32 *addr_type,
			 gss_buffer_desc *gss_addr)
{
    switch (sa->sa_family) {
#if defined(HAVE_IPV6) && defined(HAVE_KRB5_HEIMDAL)
    case AF_INET6 : {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

	gss_addr->length = 16;
	gss_addr->value  = &sin6->sin6_addr;
	*addr_type       = GSS_C_AF_INET6;
	break;
    }
#endif
    case AF_INET : {
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;

	gss_addr->length = 4;
	gss_addr->value  = &sin->sin_addr;
	*addr_type       = GSS_C_AF_INET;
	break;
    }
    default :
	ftp_err("unknown address family %d", sa->sa_family);

    }
}

/*extern struct sockaddr *hisctladdr, *myctladdr;*/
#define myctladdr ((struct sockaddr *) sock_local_addr(ftp->ctrl))
#define hisctladdr ((struct sockaddr *) sock_remote_addr(ftp->ctrl))

static int
import_name(const char *kname, const char *host, gss_name_t *target_name)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc name;

    name.length = asprintf((char**)&name.value, "%s@%s", kname, host);
    if (name.value == NULL) {
	printf("Out of memory\n");
	return AUTH_ERROR;
    }

    maj_stat = gss_import_name(&min_stat,
			       &name,
			       GSS_C_NT_HOSTBASED_SERVICE,
			       target_name);
    if (GSS_ERROR(maj_stat)) {
	OM_uint32 new_stat;
	OM_uint32 msg_ctx = 0;
	gss_buffer_desc status_string;

	gss_display_status(&new_stat,
			   min_stat,
			   GSS_C_MECH_CODE,
			   GSS_C_NO_OID,
			   &msg_ctx,
			   &status_string);
	printf("Error importing name %s: %s\n",
	       (char *)name.value,
	       (char *)status_string.value);
	gss_release_buffer(&new_stat, &status_string);
	return AUTH_ERROR;
    } else {
       void (*tracefunq)(const char *fmt, ...);
       tracefunq = (ftp->verbosity == vbDebug ? ftp_err : ftp_trace);
       tracefunq("Trying to authenticate to <%s>\n", (char *)name.value);
    }
    free(name.value);
    return 0;
}

static int
gss_auth(void *app_data, const char *host)
{
    OM_uint32 maj_stat, min_stat;
    gss_name_t target_name;
    gss_buffer_desc input, output_token;
    int context_established = 0;
    char *p;
    gss_channel_bindings_t bindings;
    struct gss_data *d = app_data;

    const char *knames[] = { "ftp", "host", NULL }, **kname = knames;

    if(import_name(*kname++, host, &target_name))
	return AUTH_ERROR;

    input.length = 0;
    input.value = NULL;

    bindings = malloc(sizeof(*bindings));

    sockaddr_to_gss_address (myctladdr,
			     &bindings->initiator_addrtype,
			     &bindings->initiator_address);
    sockaddr_to_gss_address (hisctladdr,
			     &bindings->acceptor_addrtype,
			     &bindings->acceptor_address);

    bindings->application_data.length = 0;
    bindings->application_data.value = NULL;

    while(!context_established) {
       maj_stat = gss_init_sec_context(&min_stat,
					GSS_C_NO_CREDENTIAL,
					&d->context_hdl,
					target_name,
					GSS_C_NO_OID,
                                        GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG
                                          | GSS_C_DELEG_FLAG,
					0,
					bindings,
					&input,
					NULL,
					&output_token,
					NULL,
					NULL);
	if (GSS_ERROR(maj_stat)) {
	    OM_uint32 new_stat;
	    OM_uint32 msg_ctx = 0;
	    gss_buffer_desc status_string;

	    if(min_stat == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN && *kname != NULL) {
		if(import_name(*kname++, host, &target_name))
		    return AUTH_ERROR;
		continue;
	    }

	    gss_display_status(&new_stat,
			       min_stat,
			       GSS_C_MECH_CODE,
			       GSS_C_NO_OID,
			       &msg_ctx,
			       &status_string);
	    printf("Error initializing security context: %s\n", 
		   (char*)status_string.value);
	    gss_release_buffer(&new_stat, &status_string);
	    return AUTH_CONTINUE;
	}

	gss_release_buffer(&min_stat, &input);
	if (output_token.length != 0) {
	    b64_encode(output_token.value, output_token.length, &p);
	    gss_release_buffer(&min_stat, &output_token);
	    ftp_cmd("ADAT %s", p);
	    free(p);
	}
	if (GSS_ERROR(maj_stat)) {
	    if (d->context_hdl != GSS_C_NO_CONTEXT)
		gss_delete_sec_context (&min_stat,
					&d->context_hdl,
					GSS_C_NO_BUFFER);
	    break;
	}
	if (maj_stat & GSS_S_CONTINUE_NEEDED) {
	    p = strstr(ftp->reply, "ADAT=");
	    if(p == NULL){
		printf("Error: expected ADAT in reply. got: %s\n",
		       ftp->reply);
		return AUTH_ERROR;
	    } else {
		p+=5;
		input.value = malloc(strlen(p));
		input.length = b64_decode(p, input.value);
	    }
	} else {
	    if(ftp->fullcode != 235) {
		printf("Unrecognized response code: %d\n", ftp->fullcode);
		return AUTH_ERROR;
	    }
	    context_established = 1;
	}
    }
    return AUTH_OK;
}

struct sec_client_mech gss_client_mech = {
    "GSSAPI",
    sizeof(struct gss_data),
    gss_init,
    gss_auth,
    NULL, /* end */
    gss_check_prot,
    gss_overhead,
    gss_encode,
    gss_decode,
};

/*
 * ftp.c -- low(er) level FTP stuff
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
#include "xmalloc.h"
#include "strq.h"
#include "gvars.h"
#ifdef HAVE_LIBSSH
#include "ssh_cmd.h"
#endif
#include "args.h"

/* in cmd.c */
void exit_yafc(void);

/* in tag.c */
void save_taglist(const char *alt_filename);

/* in bookmark.c */
void auto_create_bookmark(void);

Ftp *ftp = 0;

Ftp *ftp_create(void)
{
    Ftp* ftp = xmalloc(sizeof(Ftp));
    memset(ftp, 0, sizeof(Ftp));

    ftp->verbosity = vbCommand;
    ftp->tmp_verbosity = vbUnset;
    ftp->last_mkpath = 0;
    ftp->cache = list_new((listfunc)rdir_destroy);
    ftp->dirs_to_flush = list_new((listfunc)free);
    ftp->reply_timeout = 30;
    ftp->open_timeout = 30;
    ftp->taglist = list_new((listfunc)rfile_destroy);
#ifdef SECFTP
    ftp->app_data = 0;
/*  ftp->in_buffer = 0;*/
/*  ftp->out_buffer = 0;*/
#endif
    ftp->LIST_type = ltUnknown;
#ifdef HAVE_LIBSSH
		ftp->session = NULL;
		ftp->sftp_session = NULL;
#endif

    return ftp;
}

int ftp_set_trace(const char *filename)
{
    if(gvLogfp)
        /* already opened */
        return 0;
    gvLogfp = fopen(filename, "w");
    if(gvLogfp) {
        time_t now = time(0);
        setbuf(gvLogfp, 0); /* change buffering */
        fprintf(gvLogfp, "yafc " VERSION " trace file started %s\n",
                ctime(&now));
        return 0;
    }
    return -1;
}

void ftp_vtrace(const char *fmt, va_list ap)
{
    if(gvLogfp)
        vfprintf(gvLogfp, fmt, ap);
}

void ftp_trace(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ftp_vtrace(fmt, ap);
    va_end(ap);
}

void ftp_err(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    ftp_vtrace(fmt, ap);
    va_end(ap);
}

void ftp_use(Ftp *useftp)
{
    ftp = useftp;
}

void ftp_destroy(Ftp *ftp)
{
    if(!ftp)
        return;

    list_free(ftp->dirs_to_flush);
    list_free(ftp->cache);
    ftp->cache = ftp->dirs_to_flush = 0;
    host_destroy(ftp->host);
    sock_destroy(ftp->data);
    sock_destroy(ftp->ctrl);
    ftp->host = NULL;
    ftp->data = ftp->ctrl = NULL;
    url_destroy(ftp->url);
    ftp->url = NULL;
    free(ftp->homedir);
    free(ftp->curdir);
    free(ftp->prevdir);
    list_free(ftp->taglist);
    free(ftp->ti.remote_name);
    free(ftp->ti.local_name);

#ifdef SECFTP
    sec_end();
#endif

    free(ftp);
}

static int proxy_type(url_t *url)
{
    listitem *li;
    char *url_domain;

    if(gvProxyType == 0 || url->noproxy == true || gvProxyUrl == 0)
        return 0;

    url_domain = strrchr(url->hostname, '.');

    for(li=gvProxyExclude->first; li; li=li->next) {
        char *xhost = (char *)li->data;
        if(!xhost)
            /* should not happen */
            continue;

        if(xhost[0] == '.' && url_domain) {
            if(strcasecmp(url_domain, xhost) == 0)
                /* exclude domain names */
                return 0;
        }
        if(strcasecmp(xhost, "localnet") == 0 && url_domain == 0)
            /* exclude unqualified hosts */
            return 0;
        if(strcasecmp(xhost, url->hostname) == 0)
            /* exclude hostnames */
            return 0;
    }
    return gvProxyType;
}

void ftp_reset_vars(void)
{
    sock_destroy(ftp->data);
    ftp->data = NULL;

    sock_destroy(ftp->ctrl);
    ftp->ctrl = NULL;

    host_destroy(ftp->host);
    ftp->host = NULL;

#ifdef HAVE_LIBSSH
		if (ftp->session)
		{
			sftp_free(ftp->sftp_session);
			ssh_disconnect(ftp->session);
			ssh_free(ftp->session);
			ftp->sftp_session = NULL;
			ftp->session = NULL;
		}
#endif

    url_destroy(ftp->url);
    ftp->url = NULL;

    ftp->connected = false;
    ftp->loggedin = false;

    ftp->has_mdtm_command = true;
    ftp->has_size_command = true;
    ftp->has_pasv_command = true;
    ftp->has_stou_command = true;
    ftp->has_site_chmod_command = true;
    ftp->has_site_idle_command = true;
    ftp->has_mlsd_command = true;

    list_free(ftp->dirs_to_flush);
    ftp->dirs_to_flush = list_new((listfunc)free);

    list_free(ftp->cache);
    ftp->cache = list_new((listfunc)rdir_destroy);

    /* don't assume server is in ascii mode initially even if RFC says so */
    ftp->prev_type = '?';

    ftp->code = ctNone;
    ftp->fullcode = 0;

    ftp->reply_timeout = gvCommandTimeout;

    free(ftp->last_mkpath);
    ftp->last_mkpath = 0;

#ifdef SECFTP
    sec_end();
    ftp->request_data_prot = 0;
    ftp->buffer_size = 0;
#endif
    ftp->LIST_type = ltUnknown;

    list_free(ftp->taglist);
    ftp->taglist = list_new((listfunc)rfile_destroy);
}

void ftp_close(void)
{
    ftp_trace("Closing down connection...\n");
    auto_create_bookmark();
    if(gvLoadTaglist != 0) {
        save_taglist(0);
    }
    ftp_reset_vars();
}

void ftp_quit_all(void)
{
    listitem *li;

    /* nicely close all open connections */
    for(li=gvFtpList->first; li; li=li->next) {
        ftp_use((Ftp *)li->data);
        ftp_quit();
    }
}

#ifdef HAVE_POSIX_SIGSETJMP
static sigjmp_buf open_timeout_jmp;
#else
static jmp_buf open_timeout_jmp;
#endif

static void ftp_open_handler(int signum)
{
    ftp_longjmp(open_timeout_jmp, 1);
}

int ftp_open_url(url_t *urlp, bool reset_vars)
{
    bool use_proxy;
    int i;

    if(reset_vars)
        ftp_reset_vars();
    /* don't assume server is in ascii mode initially even if RFC says so */
    ftp->prev_type = '?';

#ifdef HAVE_POSIX_SIGSETJMP
    if(sigsetjmp(open_timeout_jmp, 1))
#else
    if(setjmp(open_timeout_jmp))
#endif
    {
        ftp_close();
        ftp_err(_("Connection timeout after %u seconds\n"),
                ftp->open_timeout);
        return 1;
    }
    ftp_set_signal(SIGALRM, ftp_open_handler);
    alarm(ftp->open_timeout);

    use_proxy = (proxy_type(urlp) != 0);

    ftp_err(_("Looking up %s... "),
            use_proxy ? gvProxyUrl->hostname : urlp->hostname);

    /* Set the default port (22) for SSH if no port is specified. We
     * need to do this here, 'cause host_lookup() sets it to 21
     * (default port for vanilla FTP)
     */
    if(urlp->protocol) {
        if(strcmp(urlp->protocol, "sftp") == 0)
            url_setprotocol(urlp, "ssh");
        if(strcmp(urlp->protocol, "ssh") == 0 && urlp->port == -1)
            urlp->port = 22; /* default SSH port */
    }

    ftp->host = host_create(use_proxy ? gvProxyUrl : urlp);

    if (!urlp->protocol || strcmp(urlp->protocol, "ssh") != 0) {
      // We only need to do a host lookup if we're not using sftp.
      if(!host_lookup(ftp->host)) {
        fprintf(stderr, "Host look up for '%s' failed: %s\n",
            host_getname(ftp->host), host_geterror(ftp->host));
        alarm(0);
        ftp_set_signal(SIGALRM, SIG_IGN);
        return -1;
      }
    }
    /* keep the value in urlp->port
    urlp->port = ntohs(ftp->host->port);
    and set it to 21 if it is -1 */
    if(urlp->port == -1) {
	    urlp->port = 21;
    }


    fprintf(stderr, "\r               ");
    i = strlen(use_proxy ? gvProxyUrl->hostname : urlp->hostname);
    while(i--)
        fprintf(stderr, " ");
    fprintf(stderr, "\r");
    ftp_trace("\n");

#ifdef HAVE_LIBSSH
    if(urlp->protocol && strcmp(urlp->protocol, "ssh") == 0) {
        int ret = ssh_open_url(urlp);
        alarm(0);
        return ret;
    }
#endif

    if(urlp->protocol && strcmp(urlp->protocol, "ftp") != 0) {
        ftp_err(_("Sorry, don't know how to handle your '%s' protocol\n"
                  "trying 'ftp' instead...\n"),
                urlp->protocol);
        url_setprotocol(urlp, 0);
    }

    if(use_proxy) {
        ftp_err(_("Connecting to proxy %s at port %d...\n"),
                host_getoname(ftp->host), urlp->port);
    } else {
        ftp_err(_("Connecting to %s at port %d...\n"),
                host_getoname(ftp->host), urlp->port);
    }

    ftp->ctrl = sock_create();
    if (!ftp->ctrl) {
        ftp_err(_("Unable to create socket.\n"));
        alarm(0);
        ftp_set_signal(SIGALRM, SIG_IGN);
        return -1;
    }

    if(!sock_connect_host(ftp->ctrl, ftp->host)) {
        alarm(0);
        ftp_set_signal(SIGALRM, SIG_IGN);
        return -1;
    }
    sock_lowdelay(ftp->ctrl);
    char* ip = host_getip(ftp->host);
    ftp_err(_("Connected to %s ([%s]:%d).\n"),
        host_getoname(ftp->host), ip, urlp->port);
    free(ip);

    /* read startup message from server */
    ftp_set_tmp_verbosity(vbCommand);
    ftp_read_reply();
    if(ftp->fullcode == 120) {
        ftp_set_tmp_verbosity(vbCommand);
        ftp_read_reply();
    }
    alarm(0);
    ftp_set_signal(SIGALRM, SIG_IGN);
    if(!sock_connected(ftp->ctrl)) {
        ftp_close();
        return 1;
    }
    ftp->connected = (ftp->fullcode == 220);

    if(ftp->connected) {
        void (*tracefunq)(const char *fmt, ...);

        url_destroy(ftp->url);
        ftp->url = url_clone(urlp);

        tracefunq = (ftp->verbosity == vbDebug ? ftp_err : ftp_trace);

        char* remote_addr = printable_address(sock_remote_addr(ftp->ctrl)),
            *local_addr = printable_address(sock_local_addr(ftp->ctrl));
        tracefunq("remote address: %s\n", remote_addr);
        tracefunq("local address: %s\n", local_addr);
        free(remote_addr);
        free(local_addr);

        return 0;
    } else {
        ftp_close();
        return 1;
    }
}

int ftp_reopen(void)
{
    if(ftp && ftp->url) {
        url_t *u = url_clone(ftp->url);
        int r;

        url_setdirectory(u, ftp->curdir);

        r = ftp_open_url(u, false);
        if(r == 0) {
            r = ftp_login(u->username, gvAnonPasswd);
        } else
            ftp_close();
        url_destroy(u);
        return r;
    }

    return -1;
}

/* reads one line from server into ftp->reply
 * returns 0 on success or -1 on failure
 */
static int ftp_gets(void)
{
    ftp->reply[0] = 0;

    if(!sock_connected(ftp->ctrl)) {
        ftp_err(_("No control connection\n"));
        return -1;
    }

    size_t i = 0;
    bool next = true;
    int nc = 0;
    while(true)
    {
        int c = 0;
        if (next)
          c = sock_get(ftp->ctrl);
        else
        {
          c = nc;
          next = true;
        }

        if (c == EOF)
        {
            ftp_err(_("Server has closed control connection\n"));
            ftp_close();
            return -1;
        }
        else if(c == 255/*IAC*/)
        {
            /* handle telnet commands */
            switch(c = sock_get(ftp->ctrl)) {
              case 251/*WILL*/:
              case 252/*WONT*/:
                c = sock_get(ftp->ctrl);
                sock_printf(ftp->ctrl, "%c%c%c", 255/*IAC*/, 254/*DONT*/, c);
                sock_flush(ftp->ctrl);
                break;
              case 253/*DO*/:
              case 254/*DONT*/:
                c = sock_get(ftp->ctrl);
                sock_printf(ftp->ctrl, "%c%c%c", 255/*IAC*/, 252/*WONT*/, c);
                sock_flush(ftp->ctrl);
                break;
              default:
                break;
            }
            continue;
        }
        else if (c == '\r')
        {
            nc = sock_get(ftp->ctrl);
            if (nc == 0)
              ; // do nothing
            else if (nc == '\n') {
                ftp->reply[i++] = (char)c;
                break;
            } else if(nc == EOF)
                c = EOF ;
            else
                /* telnet protocol violation, hmpf... */
                next = false;
        }
        else if (c == '\n')
            break;
        if(i < MAXREPLY)
            ftp->reply[i++] = (char)c;
    }

    if(i >= MAXREPLY) {
        ftp_err(_("Reply too long (truncated)\n"));
        i = MAXREPLY;
    }

    ftp->reply[i] = '\0';
    ftp->fullcode = atoi(ftp->reply);
#ifdef SECFTP
    {
        int r = 0;
        switch(ftp->fullcode) { /* handle protected responses 6xx */
        case 631:
            r = sec_read_msg(ftp->reply, prot_safe);
            break;
        case 632:
            r = sec_read_msg(ftp->reply, prot_private);
            break;
        case 633:
            r = sec_read_msg(ftp->reply, prot_confidential);
            break;
        }
        if(r == -1) {
            ftp->fullcode = 0;
            ftp->code = vbNone;
            return 0;
        } else
            ftp->fullcode = atoi(ftp->reply);
    }
#endif
    strip_trailing_chars(ftp->reply, "\n\r");
    ftp->code = ftp->fullcode / 100;
    return ftp->fullcode;
}

const char *ftp_getreply(bool withcode)
{
    char *r = ftp->reply;

#ifdef HAVE_LIBSSH
    if (ftp->session)
        return ssh_get_error(ftp->session);
#endif

    if(withcode)
        return r;

    if(isdigit((int)r[0]))
        r += 3;
    if(r[0] == '-' || r[0] == ' ')
        r++;
    return r;
}

static void ftp_print_reply(void)
{
    verbose_t v = ftp_get_verbosity();

    ftp_trace("<-- [%s] %s\n",
              ftp->url ? ftp->url->hostname : host_getname(ftp->host),
              ftp_getreply(true));

    if(v >= vbCommand || (ftp->code >= ctTransient && v == vbError)) {
        if(v == vbDebug)
            fprintf(stderr, "<-- [%s] %s\n",
                    ftp->url ? ftp->url->hostname : host_getname(ftp->host),
                    ftp_getreply(true));
        else
            fprintf(stderr, "%s\n", ftp_getreply(false));
    }
}

static void reply_ALRM_handler(int signum)
{
    ftp_err(_("Tired of waiting for reply, timeout after %u seconds\n"),
            ftp->reply_timeout);
    ftp_close();
    if(gvJmpBufSet) {
        ftp_trace("jumping to gvRestartJmp\n");
        ftp_longjmp(gvRestartJmp, 1);
    } else
        exit_yafc();
}

/* reads reply
 * returns 0 on success or -1 on error
 */
int ftp_read_reply(void)
{
    char tmp[5]="xxx ";
    int r;

    ftp_set_signal(SIGALRM, reply_ALRM_handler);
    if(ftp->reply_timeout)
        alarm(ftp->reply_timeout);

    sock_clearerr_in(ftp->ctrl);
    r = ftp_gets();
    if(!sock_connected(ftp->ctrl)) {
        alarm(0);
        ftp_set_signal(SIGALRM, SIG_DFL);
        ftp_trace("sock is not connected\n");
        return -1;
    }

    if(r == -1) {
        alarm(0);
        ftp_set_signal(SIGALRM, SIG_DFL);
        ftp_trace("ftp_gets returned -1\n");
        return -1;
    }

    ftp_print_reply();

    if(ftp->reply[3] == '-') {  /* multiline response */
        strncpy(tmp, ftp->reply, 3);
        do {
            if(ftp_gets() == -1)
                break;
            ftp_print_reply();
        } while(strncmp(tmp, ftp->reply, 4) != 0);
    }
    ftp->tmp_verbosity = vbUnset;
    alarm(0);
    ftp_set_signal(SIGALRM, SIG_DFL);
    return r;
}

static void ftp_print_cmd(const char *cmd, va_list ap)
{
    if(ftp_get_verbosity() == vbDebug) {
        ftp_err("--> [%s] ", ftp->url->hostname);
        if(strncmp(cmd, "PASS", 4) == 0)
            ftp_err("PASS ********");
        else {
            vfprintf(stderr, cmd, ap);
            va_list aq;
            va_copy(aq, ap);
            ftp_vtrace(cmd, aq);
            va_end(aq);
        }
        ftp_err("\n");
    } else {
        ftp_trace("--> [%s] ", ftp->url->hostname);
        if(strncmp(cmd, "PASS", 4) == 0)
            ftp_trace("PASS ********");
        else
            ftp_vtrace(cmd, ap);
        ftp_trace("\n");
    }
}

/* sends an FTP command on the control channel
 * returns reply status code on success or -1 on error
 */
int ftp_cmd(const char *cmd, ...)
{
    va_list ap;
    int resp;
    bool recon = false;

    if(!sock_connected(ftp->ctrl)) {
        ftp_err(_("No control connection\n"));
        ftp->code = ctNone;
        ftp->fullcode = -1;
        return -1;
    }

    ftp_set_abort_handler();

  ugly:

    va_start(ap, cmd);
    sock_krb_vprintf(ftp->ctrl, cmd, ap);
    sock_printf(ftp->ctrl, "\r\n");
    sock_flush(ftp->ctrl);
    va_end(ap);

    if (sock_error_out(ftp->ctrl)) {
        ftp_err(_("error writing command"));
        ftp_err(" (");
        va_start(ap, cmd);
        vfprintf(stderr, cmd, ap);
        va_end(ap);
        va_start(ap, cmd);
        ftp_vtrace(cmd, ap);
        va_end(ap);
        ftp_err(")\n");
        ftp->code = ctNone;
        ftp->fullcode = -1;
        return -1;
    }

    va_start(ap, cmd);
    ftp_print_cmd(cmd, ap);
    va_end(ap);

    resp = ftp_read_reply();
    ftp_set_close_handler();
    if(resp == 421) { /* server is closing control connection! */
        ftp_err(_("Server closed control connection\n"));
        if(gvAutoReconnect && ftp_loggedin() && strcasecmp(cmd, "QUIT") != 0) {
            if(recon) {
                ftp_err(_("Reconnect failed\n"));
            } else {
                ftp_err(_("Automatic reconnect...\n"));
                ftp_reopen();
                recon = true;
                goto ugly;
            }
        } else {
            /*      ftp_close();*/
            ftp->fullcode = 421;
            ftp->code = 4;
            return -1;
        }
    }
    return resp;
}

void ftp_quit(void)
{
#ifdef HAVE_LIBSSH
	if (ftp_connected() && !ftp->session)
#else
	if (ftp_connected())
#endif
	{
		ftp_reply_timeout(10);
    ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("QUIT");
  }
  ftp_close();
}

int ftp_get_verbosity(void)
{
    if(ftp->tmp_verbosity != vbUnset)
        return ftp->tmp_verbosity;
    return ftp->verbosity;
}

void ftp_set_verbosity(int verbosity)
{
    ftp->verbosity = verbosity;
    ftp->tmp_verbosity = vbUnset;
}

void ftp_set_tmp_verbosity(int verbosity)
{
    if(ftp->verbosity <= vbError)
        ftp->tmp_verbosity = verbosity;
}

int get_username(url_t *url, const char *guessed_username, bool isproxy)
{
    if(!url->username) {
        char *prompt, *e;

        if(!ftp->getuser_hook) {
            ftp->loggedin = false;
            return -1;
        }

        if (isproxy)
            prompt = xstrdup(_("Proxy login: "));
        else if (guessed_username)
        {
            if (asprintf(&prompt, _("login (%s): "), guessed_username) == -1)
            {
              fprintf(stderr, _("Failed to allocate memory.\n"));
              ftp->loggedin = false;
              return -1;
            }
        }
        else
            prompt = xstrdup(_("login (anonymous): "));
        e = ftp->getuser_hook(prompt);
        free(prompt);
        if(e && *e == 0) {
            free(e);
            e = 0;
        }

        if(!e) {
            if(guessed_username == 0) {
                fprintf(stderr, _("You loose\n"));
                ftp->loggedin = false;
                return -1;
            }
            url_setusername(url, guessed_username);
        } else {
            url_setusername(url, e);
            free(e);
        }
    }
    return 0;
}

int get_password(url_t *url, const char *anonpass, bool isproxy)
{
    if(!url->password) {
        char *e;

        if(url_isanon(url) && isproxy == false) {
            char *prompt;

            e = 0;
            if(ftp->getuser_hook) {
                if(anonpass && isproxy == false)
                {
                  if (asprintf(&prompt, _("password (%s): "), anonpass) == -1)
                  {
                    fprintf(stderr, _("Failed to allocate memory.\n"));
                    return -1;
                  }
                }
                else
                    prompt = xstrdup(_("password: "));
                e = ftp->getuser_hook(prompt);
                free(prompt);
            }
            if(!e || !*e) {
                free(e);
                e = xstrdup(anonpass);
            }
        } else {
            if(!ftp->getpass_hook)
                return -1;
            e = ftp->getpass_hook(isproxy ? _("Proxy password: ")
                                  : _("password: "));
        }

        if(!e) {
            fprintf(stderr, _("You loose\n"));
            return -1;
        }
        url_setpassword(url, e);
        free(e);
    }
    return 0;
}

#ifdef SECFTP

static const char *secext_name(const char *mech)
{
    static struct {
        const char *short_name;
        const char *real_name;
    } names[] = {
        {"krb4", "KERBEROS_V4"},
        {"krb5", "GSSAPI"},
        {"gssapi", "GSSAPI"},
/*      {"ssl", "SSL"},*/
        {"none", "none"},
        {0, 0}
    };
    int i;

    for(i = 0; names[i].short_name; i++) {
        if(strcasecmp(mech, names[i].short_name) == 0)
            return names[i].real_name;
    }

    return 0;
}

static bool mech_unsupported(const char *mech)
{
    if(strcasecmp(mech, "KERBEROS_V4") == 0)
        return true;
#ifndef HAVE_KRB5
    if(strcasecmp(mech, "GSSAPI") == 0)
        return true;
#endif
#ifndef USE_SSL
    if(strcasecmp(mech, "SSL") == 0)
        return true;
#endif
    return false;
}

#endif /* SECFTP */

int ftp_login(const char *guessed_username, const char *anonpass)
{
    int ptype, r;
    static url_t *purl = 0;

    if(!ftp_connected())
        return 1;

    if(!ftp->url)
        return -1;

#ifdef HAVE_LIBSSH
    if (ftp->session)
        /* login authentication is performed by the ssh program */
        return 0;
#endif

    ptype = proxy_type(ftp->url);
    if(purl) {
        url_destroy(purl);
        purl = 0;
    }
    if(ptype > 0)
        purl = url_clone(gvProxyUrl);

    r = get_username(ftp->url, guessed_username, false);
    if(r != 0)
        return r;
    if(ptype > 1 && ptype < 7) {
        r = get_username(purl, 0, true);
        if(r != 0)
            return r;
    }

#ifdef SECFTP
    ftp->sec_complete = false;
    ftp->data_prot = prot_clear;

    /* don't use secure stuff if anonymous
     */
    if(!url_isanon(ftp->url)) {
        list *mechlist;
        /* request a protection level
         */
        if(ftp->url->protlevel) {
            if(sec_request_prot(ftp->url->protlevel) != 0)
                ftp_err(_("Invalid protection level '%s'\n"),
                        ftp->url->protlevel);
        }

        /* get list of mechanisms to try
         */
        mechlist = ftp->url->mech ? ftp->url->mech : gvDefaultMechanism;
        if(mechlist) {
            listitem *li = mechlist->first;
            int ret = 0;
            for(; li; li=li->next) {
                const char *mech_name;

                mech_name = secext_name((char *)li->data);
                if(mech_name == 0) {
                    ftp_err(_("unknown mechanism '%s'\n"), (char *)li->data);
                    continue;
                }
                if(mech_unsupported(mech_name)) {
                    ftp_err(_("Yafc was not compiled with support for %s\n"),
                            mech_name);
                    continue;
                }
                ret = sec_login(host_getname(ftp->host), mech_name);
                if(ret == -1) {
                    if(ftp->code == ctError
                       && ftp->fullcode != 504 && ftp->fullcode != 534)
                        url_setmech(ftp->url, "none");
                }
                if(ret != 1)
                    break;
            }
        }
        if(ftp->sec_complete)
            ftp_err(_("Authentication successful.\n"));
        else
            ftp_err(_("*** Using plaintext username"
                      " and password ***\n"));
    }
#endif

    if(url_isanon(ftp->url))
        fprintf(stderr, _("logging in anonymously...\n"));
    ftp_set_tmp_verbosity(ftp->url->password ? vbError : vbCommand);

    switch(ptype) {
      case 0:
      default:
        ftp_cmd("USER %s", ftp->url->username);
        break;
      case 1:
        ftp_cmd("USER %s@%s", ftp->url->username, ftp->url->hostname);
        break;
      case 2:
      case 3:
      case 4:
        ftp_cmd("USER %s", purl->username);
        if(ftp->code == ctContinue) {
            r = get_password(purl, 0, true);
            if(r != 0)
                return 0;
            ftp_cmd("PASS %s", purl->password);
            /* FIXME: what reply code do we expect now? */
            if(ftp->code < ctTransient) {
                if(ptype == 2) {
                    ftp_cmd("USER %s@%s",
                            ftp->url->username, ftp->url->hostname);
                } else {
                    if(ptype == 3)
                        ftp_cmd("SITE %s", purl->hostname);
                    else
                        ftp_cmd("OPEN %s", purl->hostname);
                    if(ftp->code < ctTransient)
                        ftp_cmd("USER %s", ftp->url->username);
                }
            }
        }
        break;
      case 5:
        ftp_cmd("USER %s@%s@%s",
                ftp->url->username, purl->username, ftp->url->hostname);
        break;
      case 6:
        ftp_cmd("USER %s@%s", purl->username, ftp->url->hostname);
        if(ftp->code == ctContinue) {
            r = get_password(purl, 0, true);
            if(r != 0)
                return 0;
            ftp_cmd("PASS %s", purl->password);
            if(ftp->code < ctTransient)
                ftp_cmd("USER %s", ftp->url->username);
        }
        break;
      case 7:
        ftp_cmd("USER %s@%s:%i", ftp->url->username, ftp->url->hostname, ftp->url->port);
        break;
    }

    if(ftp->code == ctContinue) {
        ftp->loggedin = false;
        r = get_password(ftp->url, anonpass, false);
        if(r != 0)
            return r;
        if(ptype == 5) {
            r = get_password(purl, 0, true);
            if(r != 0) {
                url_destroy(purl);
                purl = 0;
                return 0;
            }
        }

        ftp_set_tmp_verbosity(vbCommand);
        switch(ptype) {
          default:
          case 0:
          case 1:
          case 2:
          case 3:
          case 4:
          case 6:
            ftp_cmd("PASS %s", ftp->url->password);
            break;
          case 5:
            ftp_cmd("PASS %s@%s", ftp->url->password, purl->password);
            break;

        }
    }

    url_destroy(purl);
    purl = 0;

    if(ftp->code > ctContinue) {
        if(ftp->fullcode == 530 && ftp_loggedin()) {
            /* this probable means '530 Already logged in' */
            return 2;
        }
        ftp->loggedin = false;
        return 1;
    }
    if(ftp->code == ctComplete) {
        ftp->loggedin = true;
#ifdef SECFTP
        /* we are logged in, now set the requested data protection level
         * requested from the autologin information in the config file,
         * if any, else uses default protection level 'clear', ie
         * no protection on the data channel
         */
        if(ftp->sec_complete) {
            sec_set_protection_level();
            fprintf(stderr, _("Data protection is %s\n"),
                    level_to_name(ftp->data_prot));
        }
#endif

        ftp->homedir = ftp_getcurdir();
        ftp->curdir = xstrdup(ftp->homedir);
        ftp->prevdir = xstrdup(ftp->homedir);
        if(ftp->url->directory)
            ftp_chdir(ftp->url->directory);
        ftp_get_feat();
        return 0;
    }
    if(ftp->code == ctTransient)
        return 1;
    return -1;
}

bool ftp_loggedin(void)
{
    return (ftp_connected() && ftp->loggedin);
}

bool ftp_connected(void)
{
#ifdef HAVE_LIBSSH
	return (ftp->connected && (sock_connected(ftp->ctrl) || ftp->session));
#else
	return (ftp->connected && sock_connected(ftp->ctrl));
#endif
}

char *ftp_getcurdir(void)
{
#ifdef HAVE_LIBSSH
    if (ftp->session)
        return ssh_getcurdir();
#endif

    ftp_set_tmp_verbosity(vbNone);
    ftp_cmd("PWD");
    if(ftp->code == ctComplete) {
        char *beg, *end, *ret;
        beg = strchr(ftp->reply, '\"');
        if(!beg)
            return xstrdup("CWD?");
        beg++;
        end = strchr(beg, '\"');
        if(!end)
            return xstrdup("CWD?");
        ret = (char *)xmalloc(end-beg+1);
        strncpy(ret, beg, end-beg);
        stripslash(ret);
        /* path shouldn't include any quoted chars */
        path_dos2unix(ret);
        return ret;
    }
    return xstrdup("CWD?");
}

void ftp_update_curdir_x(const char *p)
{
    free(ftp->prevdir);
    ftp->prevdir = ftp->curdir;
    ftp->curdir = xstrdup(p);
    path_dos2unix(ftp->curdir);
}

static void ftp_update_curdir(void)
{
    free(ftp->prevdir);
    ftp->prevdir = ftp->curdir;
    ftp->curdir = ftp_getcurdir();
}

int ftp_chdir(const char *path)
{
#ifdef HAVE_LIBSSH
    if (ftp->session)
        return ssh_chdir(path);
#endif

    ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("CWD %s", path);
    if(ftp->code == ctComplete) {
        /* Now, try to be smart ;-)
         * Many ftp servers include the current directory in the CWD reply
         * try to parse it, so we don't need to issue a PWD command
         */

        if(strncasecmp(ftp->reply, "250 Changed to ", 15) == 0) {
            /* this is what Troll-ftpd replies: 250 Changed to /foo/bar */
            ftp_update_curdir_x(ftp->reply+15);
            ftp_trace("Parsed cwd '%s' from reply\n", ftp->curdir);
        } else if(strncasecmp(ftp->reply,
                              "250 OK. Current directory is ", 29) == 0) {
            /* PureFTPd responds: "250 OK. Current directory is /foo/bar */
            ftp_update_curdir_x(ftp->reply+29);
            ftp_trace("Parsed cwd '%s' from reply\n", ftp->curdir);
        } else if(strstr(ftp->reply, " is current directory") != 0) {
            /* WarFTPD answers: 250 "/foo/bar/" is current directory */
            char *edq;
            char *sdq = strchr(ftp->reply, '\"');
            if(sdq) {
                edq = strchr(sdq+1, '\"');
                if(edq) {
                    char *e = xstrndup(sdq+1, edq-sdq-1);
                    stripslash(e);
                    ftp_update_curdir_x(e);
                    free(e);
                    ftp_trace("Parsed cwd '%s' from reply\n", ftp->curdir);
                }
            }
        } else if(strncasecmp(ftp->reply,
                              "250 Directory changed to ", 25) == 0) {
            /* Serv-U FTP Server for WinSock */
            ftp_update_curdir_x(ftp->reply + 25);
            ftp_trace("Parsed cwd '%s' from reply\n", ftp->curdir);
        } else
            ftp_update_curdir();
        return 0;
    }
    return -1;
}

int ftp_cdup(void)
{
#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_cdup();
#endif

    ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("CDUP");
    if(ftp->code == ctComplete) {
        ftp_update_curdir();
        return 0;
    }
    return -1;
}

static int ftp_mkdir_verb(const char *path, verbose_t verb)
{
    char *p;

#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_mkdir_verb(path, verb);
#endif

    p = xstrdup(path);
    stripslash(p);

    ftp_set_tmp_verbosity(verb);
    ftp_cmd("MKD %s", p);
    if(ftp->code == ctComplete)
        ftp_cache_flush_mark_for(p);
    free(p);
    return ftp->code == ctComplete ? 0 : -1;
}

int ftp_mkdir(const char *path)
{
    return ftp_mkdir_verb(path, vbError);
}

int ftp_rmdir(const char *path)
{
    char *p;

#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_rmdir(path);
#endif

    p = xstrdup(path);
    stripslash(p);
    ftp_set_tmp_verbosity(vbError);
    ftp_cmd("RMD %s", p);
    if(ftp->code == ctComplete) {
        ftp_cache_flush_mark(p);
        ftp_cache_flush_mark_for(p);
    }
    free(p);
    return ftp->code == ctComplete ? 0 : -1;
}

int ftp_unlink(const char *path)
{
#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_unlink(path);
#endif

    ftp_cmd("DELE %s", path);
    if(ftp->code == ctComplete) {
        ftp_cache_flush_mark_for(path);
        return 0;
    }
    return -1;
}

int ftp_chmod(const char *path, const char *mode)
{
#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_chmod(path, mode);
#endif

    if(ftp->has_site_chmod_command) {
        ftp_set_tmp_verbosity(vbNone);
        ftp_cmd("SITE CHMOD %s %s", mode, path);
        if(ftp->fullcode == 502)
            ftp->has_site_chmod_command = false;
        if(ftp->code == ctComplete) {
            ftp_cache_flush_mark_for(path);
            return 0;
        }
    } else
        ftp_err(_("Server doesn't support SITE CHMOD\n"));

    return -1;
}

void ftp_reply_timeout(unsigned int secs)
{
    ftp->reply_timeout = secs;
}

int ftp_idle(const char *idletime)
{
#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_idle(idletime);
#endif

    if(!ftp->has_site_idle_command) {
        ftp_err(_("Server doesn't support SITE IDLE\n"));
        return -1;
    }

    ftp_set_tmp_verbosity(vbCommand);
    if(idletime)
        ftp_cmd("SITE IDLE %s", idletime);
    else
        ftp_cmd("SITE IDLE");

    if(ftp->fullcode == 502)
        ftp->has_site_idle_command = false;
    return ftp->code == ctComplete ? 0 : -1;
}

int ftp_noop(void)
{
#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_noop();
#endif

    ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("NOOP");
    return ftp->code == ctComplete ? 0 : -1;
}

int ftp_help(const char *arg)
{
#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_help(arg);
#endif

    ftp_set_tmp_verbosity(vbCommand);
    if(arg)
        ftp_cmd("HELP %s", arg);
    else
        ftp_cmd("HELP");
    return ftp->code == ctComplete ? 0 : -1;
}

unsigned long long ftp_filesize(const char *path)
{
    unsigned long long ret;

#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_filesize(path);
#endif

    if(!ftp->has_size_command)
        return -1;
    if(ftp_type(tmBinary) != 0)
        return -1;
    ftp_set_tmp_verbosity(vbError);
    ftp_cmd("SIZE %s", path);
    if(ftp->fullcode == 502) {
        ftp->has_size_command = false;
        return -1;
    }
    if(ftp->code == ctComplete) {
        sscanf(ftp->reply, "%*s %llu", &ret);
        return ret;
    }
    return -1;
}

rdirectory *ftp_read_directory(const char *path)
{
    FILE *fp = 0;
    rdirectory *rdir;
    bool is_curdir = false;
    bool _failed = false;
    char *dir;
    bool is_mlsd = false;

#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_read_directory(path);
#endif

    dir = ftp_path_absolute(path);
    stripslash(dir);

    is_curdir = (strcmp(dir, ftp->curdir) == 0);

    if((fp = tmpfile()) == NULL) {	/* can't create a tmpfile */
	    ftp_err("Unable to create temp file: %s\n", strerror(errno));
            free(dir);
            return 0;
    }

    /* we do a "CWD" before the listing, because: we want a listing of
     *  the directory contents, not the directory itself, and some
     *  servers misunderstand this. If the target is a link to a
     *  directory, we have to do this.
     */
    if(!is_curdir) {
        ftp_cmd("CWD %s", dir);
        if(ftp->code != ctComplete)
            goto failed;
    }

    if(ftp->has_mlsd_command) {
        is_mlsd = true;
#if 0
        /* PureFTPd (1.0.11) doesn't recognize directory arguments
         * with spaces, not even quoted, it just chops the argument
         * string after the first space, duh... so we have to CWD to
         * the directory...
         */
        char *asdf;
        asprintf(&asdf, "%s/", dir);
        /* Hack to get around issue in PureFTPd (up to version 0.98.2):
         * doing a 'MLSD link-to-dir' on PureFTPd closes the control
         * connection, however, 'MLSD link-to-dir/' works fine.
         */
        _failed = (ftp_list("MLSD", asdf, fp) != 0);
        free(asdf);
#else
        _failed = (ftp_list("MLSD", 0, fp) != 0);
#endif
        if(_failed && ftp->code == ctError)
            ftp->has_mlsd_command = false;
    }
    if(!ftp->has_mlsd_command) {
        _failed = (ftp_list("LIST", 0, fp) != 0);
        is_mlsd = false;
    }

    if(!is_curdir)
        ftp_cmd("CWD %s", ftp->curdir);

    if(_failed)
        goto failed;

    rewind(fp);

    rdir = rdir_create();
    if(rdir_parse(rdir, fp, dir, is_mlsd) != 0) {
        rdir_destroy(rdir);
        goto failed;
    }

    fclose(fp);
    ftp_trace("added directory '%s' to cache\n", dir);
    list_additem(ftp->cache, rdir);
    free(dir);
    return rdir;

  failed: /* forgive me father, for I have goto'ed */
    if (fp)
	    fclose(fp);
    free(dir);
    return 0;
}

rdirectory *ftp_get_directory(const char *path)
{
    rdirectory *rdir;
    char *ap;

    ap = ftp_path_absolute(path);
    stripslash(ap);

    rdir = ftp_cache_get_directory(ap);
    if(!rdir)
        rdir = ftp_read_directory(ap);
    free(ap);
    return rdir;
}

/* returns the rfile at PATH
 * if it's not in the cache, reads the directory
 * returns 0 if not found
 */
rfile *ftp_get_file(const char *path)
{
    rfile *f;
    char *ap;

    if(!path)
        return 0;

    ap = ftp_path_absolute(path);
    stripslash(ap);

    f = ftp_cache_get_file(ap);
    if(!f) {
        char *p = base_dir_xptr(ap);
        rdirectory *rdir = ftp_get_directory(p);
        free(p);
        if(rdir)
            f = rdir_get_file(rdir, base_name_ptr(ap));
    }
    free(ap);
    return f;
}

/* returns true if path A is part of path B
 */
static bool ftp_path_part_of(const char *a, const char *b)
{
    size_t alen;

    if(!a || !b)
        return false;

    alen = strlen(a);

    /* see if a and b are equal to the length of a */
    if(strncmp(a, b, alen) == 0) {
        /* see if the last directory in b is complete */
        if(strlen(b) >= alen) {
            char c = b[alen];
            if(c == 0 || c == '/') {
/*              ftp_trace("directory %s already created\n", a);*/
                return true;
            }
        }
    }

    return false;
}

/* creates path (and all elements in path)
 * PATH should be an absolute path
 * returns -1 on error, 0 if no directories created, else 1
 */
int ftp_mkpath(const char *path)
{
    bool one_created = false;
    char *p, *orgp, *e = NULL;

    if(!path)
        return 0;

    /* check if we already has created this path */
    if(ftp_path_part_of(path, ftp->last_mkpath))
        return 0;

    /* check if this path is a part of current directory */
    if(ftp_path_part_of(path, ftp->curdir))
        return 0;

    orgp = p = xstrdup(path);
    path_collapse(p);
    unquote(p);

    if(*p == '/') {
        e = xmalloc(1);
        *e = 0;
    }

    while(true) {
        char* tmp = strqsep(&p, '/');
        if(!tmp)
            break;

        char* foo = NULL;
        if (e)
        {
            if (asprintf(&foo, "%s/%s", e, tmp) == -1)
            {
              fprintf(stderr, _("Failed to allocate memory.\n"));
              free(e);
              free(orgp);
              return -1;
            }
        }
        else
            foo = xstrdup(tmp);

        free(e);
        e = foo;

        /* check if we already has created this path */
        if(ftp_path_part_of(e, ftp->last_mkpath))
            continue;

        /* check if this path is a part of current directory */
        if(ftp_path_part_of(e, ftp->curdir))
            continue;

        if(strncmp(e, ".", 2) != 0) {
            ftp_mkdir_verb(e, vbNone);
            one_created = (ftp->code == ctComplete);
        }
    }

    free(ftp->last_mkpath);
    ftp->last_mkpath = path_absolute(path, ftp->curdir, ftp->homedir);

    free(e);
    free(orgp);
    return one_created;
}

char *ftp_path_absolute(const char *path)
{
    return path_absolute(path, ftp->curdir, ftp->homedir);
}

void ftp_flush_reply(void)
{
    if(!ftp_connected())
        return;

#ifdef HAVE_LIBSSH
    if (ftp->session)
        return;
#endif

/*  ftp_set_signal(SIGINT, SIG_IGN);*/
    fprintf(stderr, "flushing replies...\r");

/*  ftp_reply_timeout(10);*/

    while(ftp_connected()) {
        if (sock_check_pending(ftp->ctrl, false) == 1)
            ftp_read_reply();
        else
            break;
    }
#if 0
    if(ftp_loggedin())
        ftp_chdir(ftp->curdir);
#endif
    ftp_set_close_handler();
}

int ftp_rename(const char *oldname, const char *newname)
{
    char *on;
    char *nn;

#ifdef HAVE_LIBSSH
    if (ftp->session)
        return ssh_rename(oldname, newname);
#endif

    on = xstrdup(oldname);
    stripslash(on);
    ftp_cmd("RNFR %s", on);
    if(ftp->code != ctContinue) {
        free(on);
        return -1;
    }

    nn = xstrdup(newname);
    stripslash(nn);
    ftp_cmd("RNTO %s", nn);
    if(ftp->code != ctComplete) {
        free(on);
        free(nn);
        return -1;
    }

    ftp_cache_flush_mark_for(on);
    ftp_cache_flush_mark_for(nn);
    return 0;
}

/*
** Function:    bool isLeapYear(int y)
**
** Description: Local routine used by gmt_mktime() below to determine
**      which years are leap years.
**
** Returns; true or false.
*/
static bool isLeapYear(int y)
{
    return (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0));
}

/*
** Function:    time_t gmt_mktime(const struct tm *ts)
**
** Description: Local routine used by ftp_filetime to convert a struct tm
**      to a GMT time_t.  mktime() is useless since it assumes
**      struct tm is a local time.
**
** Returns; Either -1 upon failure or the GMT time_t
**      time upon success.
*/
/* contributed by Charles Box */
time_t gmt_mktime(const struct tm *ts)
{
    const int SECS_MIN = 60;
    const int SECS_HOUR = SECS_MIN * 60;
    const int SECS_DAY = SECS_HOUR * 24;
    const int SECS_YEAR = SECS_DAY * 365;
    const int daysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30,
        31, 30, 31 };
    const int leapDaysPerMonth[] = { 31, 29, 31, 30, 31,
        30, 31, 31, 30, 31, 30, 31 };
    const int *maxDaysInMonth = daysPerMonth;

    time_t gmt = -1;

    int y, m;

    /* watch out for bogus date!!! */
    if (!ts)
        return gmt;
    else if (ts->tm_year < 70 || ts->tm_year > 138)
        return gmt;
    else if (ts->tm_mon < 0 || ts->tm_mon > 11)
        return gmt;

    if (isLeapYear(ts->tm_year + 1900))
        maxDaysInMonth = leapDaysPerMonth;

    if (ts->tm_mday < 1 || ts->tm_mday > maxDaysInMonth[ts->tm_mon])
        return gmt;
    else if (ts->tm_hour < 0 || ts->tm_hour > 23)
        return gmt;
    else if (ts->tm_min < 0 || ts->tm_min > 59)
        return gmt;
    else if (ts->tm_sec < 0 || ts->tm_sec > 59)
        return gmt;

    /* add in # secs for years past */
    gmt = (ts->tm_year - 70) * SECS_YEAR;
    /* add in # leap year days not including this year! */
    for (y = 1970; y < (ts->tm_year + 1900); ++y)
    {
        if (isLeapYear(y))
            gmt += SECS_DAY;
    }
    /* add in secs for all months this year excluding this month */
    for (m = 0; m < ts->tm_mon; ++m)
        gmt += maxDaysInMonth[m] * SECS_DAY;

    /* add in secs for all days this month excluding today */
    gmt += (ts->tm_mday - 1) * SECS_DAY;
    /* add in hours today */
    gmt += ts->tm_hour * SECS_HOUR;
    /* add in minutes today */
    gmt += ts->tm_min * SECS_MIN;
    /* add in secs today */
    gmt += ts->tm_sec;
    return gmt;
}

/*
** Function:    time_t ftp_filetime(const char *filename)
**
** Description: User routine for attempting to obtain a remote file's
**      modification time in Universal Coordinated Time (GMT).
**
** Returns; Either -1 upon failure or the time_t file modification
**      time upon success.
*/
time_t ftp_filetime(const char *filename)
{
    struct tm ts;

    if(!ftp_connected())
        return -1;

#ifdef HAVE_LIBSSH
    if(ftp->session)
        return ssh_filetime(filename);
#endif

    if(!ftp->has_mdtm_command)
        return -1;

    memset(&ts, 0, sizeof(ts));
    ftp_set_tmp_verbosity(vbNone);
    ftp_cmd("MDTM %s", filename);
    if (ftp->fullcode == 202) {
        ftp->has_mdtm_command = false;
        return -1;
    }
    if (ftp->fullcode != 213)
        return -1;
    /* time is Universal Coordinated Time */
    sscanf(ftp->reply, "%*s %04d%02d%02d%02d%02d%02d", &ts.tm_year,
           &ts.tm_mon, &ts.tm_mday, &ts.tm_hour, &ts.tm_min, &ts.tm_sec);
    ts.tm_year -= 1900;
    ts.tm_mon--;
    return gmt_mktime(&ts);
}

int ftp_maybe_isdir(rfile *fp)
{
    if(risdir(fp))
        return 1;

    if(rislink(fp)) {
        /* found a link; if the link is in the cache,
         * check if it's a directory, else we don't
         * read the directory just to check if it's a
         * directory
         */
        char *adir = base_dir_xptr(fp->path);
        char *ap = path_absolute(fp->link, adir, ftp->homedir);
        rfile *lnfp = ftp_cache_get_file(ap);
        free(adir);
        free(ap);
        if(lnfp)
            return risdir(lnfp) ? 1 : 0;
        else
            /* return maybe ;-) */
            return 2;
    }
    return 0;
}

void ftp_pwd(void)
{
#ifdef HAVE_LIBSSH
    if (ftp->session)
		{
        ssh_pwd();
        return;
    }
#endif

    ftp_set_tmp_verbosity(vbCommand);
    ftp_cmd("PWD");
}


char *perm2string(int perm)
{
    char* attr = xmalloc(11);
    strncpy(attr, "----------", 11);

    if(S_ISDIR(perm))
        attr[0] = 'd';
    else if(S_ISLNK(perm))
        attr[0] = 'l';
    else if(S_ISCHR(perm))
        attr[0] = 'c';
    else if(S_ISBLK(perm))
        attr[0] = 'b';
    else if(S_ISFIFO(perm))
        attr[0] = 'p';
    else if(S_ISSOCK(perm))
        attr[0] = 's';

    if(test(perm, S_IRUSR)) attr[1] = 'r';
    if(test(perm, S_IWUSR)) attr[2] = 'w';
    if(test(perm, S_IXUSR)) {
        if(test(perm, S_ISUID)) attr[3] = 's';
        else attr[3] = 'x';
    } else if(test(perm, S_ISUID)) attr[3] = 'S';

    if(test(perm, S_IRGRP)) attr[4] = 'r';
    if(test(perm, S_IWGRP)) attr[5] = 'w';
    if(test(perm, S_IXGRP)) {
        if(test(perm, S_ISGID)) attr[6] = 's';
        else attr[6] = 'x';
    } else if(test(perm, S_ISGID)) attr[6] = 'S';

    if(test(perm, S_IROTH)) attr[7] = 'r';
    if(test(perm, S_IWOTH)) attr[8] = 'w';
    if(test(perm, S_IXOTH)) {
        if(test(perm, S_ISVTX)) attr[9] = 't';
        else attr[9] = 'x';
    } else if(test(perm, S_ISVTX)) attr[9] = 'T';

    return attr;
}

void ftp_get_feat(void)
{
/*  ftp_cmd("FEAT");*/
}

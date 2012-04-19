/*
 * socket.c --
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
#include "socket.h"
#include "xmalloc.h"

Socket *sock_create(void)
{
#if 0
	int on = 1;
#endif
	int tfd;
	Socket *sockp;

	sockp = (Socket *)xmalloc(sizeof(Socket));

	sockp->remote_addr.sin_family = AF_INET;

	/* create a socket file descriptor */
	sockp->handle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockp->handle == -1)
		goto err0;
#if 0
	/* enable local address reuse */
	if(setsockopt(sockp->handle, SOL_SOCKET, SO_REUSEADDR,
				  &on, sizeof(on)) == -1)
	{
		goto err1;
	}
#endif
	if (-1 == (tfd = dup(sockp->handle))) {
		goto err1;
 	}
/*	sockp->sin = fdopen(sockp->handle, "r");
	if(sockp->sin == 0) {
		goto err1;
	}*/

	if(0 == (sockp->sin = fdopen(tfd, "r"))) {
		close(tfd);
		goto err1;
 	}

/*	sockp->sout = fdopen(sockp->handle, "w");
	if(sockp->sout == 0) {
		goto err1;
	}*/

	if (-1 == (tfd = dup(sockp->handle))) {
		goto err2;
	}
	if (0 == (sockp->sout = fdopen(tfd, "w"))) {
		close(tfd);
		goto err2;
	}
	
	return sockp;

err2:
	fclose(sockp->sin);
err1:
	close(sockp->handle);
err0:
	free(sockp);
	return 0;
}

void sock_destroy(Socket *sockp)
{
	if(!sockp)
		return;

	fclose(sockp->sin);
	if (sockp->sin != sockp->sout)		/* 853836 */
		fclose(sockp->sout);
	close(sockp->handle);
	free(sockp);
}

int sock_connect_addr(Socket *sockp, struct sockaddr_in *sa)
{
	/* connect to the socket */
	if(connect(sockp->handle, (struct sockaddr *)sa,
			   sizeof(struct sockaddr)) == -1)
	{
		perror("connect()");
		close(sockp->handle);
		return -1;
	}

	if(sock_getsockname(sockp, &sockp->local_addr) == -1) {
		perror("getsockname()");
		close(sockp->handle);
		return -1;
	}
	sockp->connected = true;

	return 0;
}

int sock_connect(Socket *sockp)
{
	return sock_connect_addr(sockp, &sockp->remote_addr);
}

int sock_connect_host(Socket *sockp, Host *hp)
{
	sockp->remote_addr.sin_port = hp->port;
	if(hp->hep) {
		memcpy((void *)&sockp->remote_addr.sin_addr,
			   hp->hep->h_addr,
			   hp->hep->h_length);
	} else {
		/* we don't have the hostname, only an ip number */
		memcpy((void *)&sockp->remote_addr.sin_addr,
			   &hp->alt_h_addr,   /* this is a struct in_addr */
			   hp->alt_h_length);
	}
	sockp->remote_addr.sin_family = AF_INET;

	return sock_connect(sockp);
}

void sock_copy(Socket *tosock, const Socket *fromsock)
{
	memcpy(&tosock->remote_addr, &fromsock->remote_addr,
		   sizeof(tosock->remote_addr));
	memcpy(&tosock->local_addr, &fromsock->local_addr,
		   sizeof(tosock->local_addr));
	tosock->connected = sock_connected(fromsock);
}

bool sock_connected(const Socket *sockp)
{
	return sockp && sockp->connected;
}

/* accept an incoming connection */
int sock_accept(Socket *sockp, const char *mode, bool pasvmode)
{
	int s;
	struct sockaddr sa;
	socklen_t l = sizeof(sa);
	int tfd;

	if(!pasvmode) {
		s = accept(sockp->handle, &sa, &l);
		close(sockp->handle);
		if(s == -1) {
			perror("accept()");
			sockp->handle = -1;
			return -1;
		}
		sockp->handle = s;
		memcpy(&sockp->local_addr, &sa, sizeof(sockp->local_addr));
	}
	fclose(sockp->sin);
	fclose(sockp->sout);

	if (-1 == (tfd = dup(sockp->handle))) {
		perror("dup()");
		goto errdup0;
	}
	if (!(sockp->sin = fdopen(tfd, mode))) {
		close(tfd);
		perror("fdopen()");
		goto errdup0;
	}

	if (-1 == (tfd = dup(sockp->handle))) {
		perror("dup()");
		goto errdup1;
	}
	if (!(sockp->sout = fdopen(tfd, mode))) {
		close(tfd);
		perror("fdopen()");
		goto errdup1;
	}

	return 0;

errdup1:
	fclose(sockp->sin);
errdup0:
	close(sockp->handle);
	sockp->handle = -1;
	return -1;
}

int sock_listen(Socket *sockp)
{
	unsigned int l = sizeof(sockp->local_addr);
	sockp->local_addr.sin_port = 0;  /* let system pick port */

	if(bind(sockp->handle, (struct sockaddr *)&sockp->local_addr, l) == -1)
	{
		perror("bind()");
		return -1;
	}

	/* wait for an incoming connection */
	if(listen(sockp->handle, 1) == -1)
		return -1;

	if(sock_getsockname(sockp, &sockp->local_addr) == -1) {
		perror("getsockname()");
		return -1;
	}

	return 0;
}

void sock_throughput(Socket *sockp)
{
#if defined(IPTOS_THROUGHPUT) && defined(HAVE_SETSOCKOPT)
	/* set type of service */
	int tos = IPTOS_THROUGHPUT;
	if(setsockopt(sockp->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
		perror(_("setsockopt(THROUGHPUT)... ignored"));
#endif
}

void sock_lowdelay(Socket *sockp)
{
#if defined(IPTOS_LOWDELAY) && defined(HAVE_SETSOCKOPT)
	/* set type of service */
	int tos = IPTOS_LOWDELAY;
	if(setsockopt(sockp->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
		perror(_("setsockopt(LOWDELAY)... ignored"));
#endif
}

const struct sockaddr_in *sock_local_addr(Socket *sockp)
{
	return &sockp->local_addr;
}

const struct sockaddr_in *sock_remote_addr(Socket *sockp)
{
	return &sockp->remote_addr;
}

ssize_t sock_read(Socket *sockp, void *buf, size_t num)
{
	return read(sockp->handle, buf, num);
}

ssize_t sock_write(Socket *sockp, void *buf, size_t num)
{
	return write(sockp->handle, buf, num);
}

int sock_get(Socket *sockp)
{
	return fgetc(sockp->sin);
}

int sock_put(Socket *sockp, int c)
{
	if(fputc(c, sockp->sout) == EOF)
		return EOF;
	return 0;
}

int sock_unget(Socket *sockp, int c)
{
	return ungetc(c, sockp->sin);
}

int sock_vprintf(Socket *sockp, const char *str, va_list ap)
{
	return vfprintf(sockp->sout, str, ap);
}

int sock_krb_vprintf(Socket *sockp, const char *str, va_list ap)
{
#ifdef SECFTP
	if(ftp->sec_complete)
		return sec_vfprintf(sockp->sout, str, ap);
	else
#endif
		return sock_vprintf(sockp, str, ap);
}

int sock_printf(Socket *sockp, const char *str, ...)
{
	va_list ap;
	int r;

	va_start(ap, str);
	r = sock_vprintf(sockp, str, ap);
	va_end(ap);
	return r;
}

int sock_krb_printf(Socket *sockp, const char *str, ...)
{
	va_list ap;
	int r;

	va_start(ap, str);
	r = sock_krb_vprintf(sockp, str, ap);
	va_end(ap);
	return r;
}

/* flushes output */
int sock_flush(Socket *sockp)
{
	return fflush(sockp->sout);
}

int sock_telnet_interrupt(Socket *sockp)
{
	if(send(sockp->handle, "\xFF\xF4\xFF" /* IAC,IP,IAC */, 3, MSG_OOB) != 3)
		return -1;
	if(fputc(242/*DM*/, sockp->sout) == EOF)
		return -1;
	return 0;
}

int sock_getsockname(Socket *sockp, struct sockaddr_in *sa)
{
	unsigned int len = sizeof(struct sockaddr_in);
	if(getsockname(sockp->handle, (struct sockaddr *)sa, &len) == -1)
		return -1;
	return 0;
}

/*
 * ftpsend.c -- send/receive files and file listings
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
#include "ftpsigs.h"
#include "gvars.h"
#ifdef HAVE_LIBSSH
#include "ssh_cmd.h"
#endif

static bool is_private(struct sockaddr* sa)
{
  if (sa->sa_family == AF_INET)
  {
    struct sockaddr_in* in = (struct sockaddr_in*) sa;
    unsigned char* addr = (unsigned char*) &in->sin_addr.s_addr;
    return (addr[0] == 10 ||
        (addr[0] == 172 && addr[1] >= 16 && addr[1] < 32) ||
        (addr[0] == 192 && addr[1] == 168));
  }
  return false;
}

static bool is_multicast(struct sockaddr* sa)
{
  if (sa->sa_family == AF_INET)
  {
    struct sockaddr_in* in = (struct sockaddr_in*) sa;
    unsigned char* addr = (unsigned char*) &in->sin_addr.s_addr;
    return (addr[0] >= 224 && addr[0] < 240);
  }
  return false;
}

static bool is_loopback(struct sockaddr* sa)
{
  if (sa->sa_family == AF_INET)
  {
    struct sockaddr_in* in = (struct sockaddr_in*) sa;
    unsigned char* addr = (unsigned char*) &in->sin_addr.s_addr;
    return (addr[0] >= 127 && addr[1] == 0 && addr[2] == 0 && addr[3] == 1);
  }
  return false;
}


static bool is_reserved(struct sockaddr* sa)
{
  if (sa->sa_family == AF_INET)
  {
    struct sockaddr_in* in = (struct sockaddr_in*) sa;
    unsigned char* addr = (unsigned char*) &in->sin_addr.s_addr;
    return (addr[0] == 0 ||
        (addr[0] == 127 && !is_loopback(sa)) ||
        addr[0] >= 240);
  }
  return false;
}

static bool ftp_pasv(bool ipv6, unsigned char* result, unsigned short* ipv6_port)
{
  if (!ftp->has_pasv_command) {
    ftp_err(_("Host doesn't support passive mode\n"));
    return false;
  }
  ftp_set_tmp_verbosity(vbNone);

  /* request passive mode */
  if (!ipv6)
    ftp_cmd("PASV");
#ifdef HAVE_IPV6
  else if (ipv6)
    ftp_cmd("EPSV");
#endif
  else
    return false;

  if (!ftp_connected())
    return false;

  if (ftp->code != ctComplete) {
    ftp_err(_("Unable to enter passive mode\n"));
    if(ftp->code == ctError) /* no use try it again */
      ftp->has_pasv_command = false;
    return false;
  }

  const char* e = ftp_getreply(false);
  while (e && !isdigit((int)*e))
    e++;

  if (!e)
  {
    ftp_err(_("Error parsing EPSV/PASV reply: '%s'\n"), ftp_getreply(false));
    return false;
  }

  if (!ipv6)
  {
    if (sscanf(e, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu", &result[0], &result[1],
          &result[2], &result[3], &result[4], &result[5]) != 6)
    {
      ftp_err(_("Error parsing PASV reply: '%s'\n"), ftp_getreply(false));
      return false;
    }
  }
#ifdef HAVE_IPV6
  else
  {
    if (sscanf(e, "%hu", ipv6_port) != 1)
    {
      ftp_err(_("Error parsing EPSV reply: '%s'\n"), ftp_getreply(false));
      return false;
    }
  }
#endif

  return true;
}

static bool ftp_is_passive(void)
{
	if(!ftp || !ftp->url || ftp->url->pasvmode == -1)
		return gvPasvmode;
	return ftp->url->pasvmode;
}

static int ftp_init_transfer(void)
{
  if (!ftp_connected())
    return -1;

  if (!sock_dup(ftp->ctrl, &ftp->data))
    return -1;

  if (ftp_is_passive())
  {
    ftp_trace("Initializing passive connection.\n");

    struct sockaddr_storage sa;
    memcpy(&sa, sock_remote_addr(ftp->ctrl), sizeof(struct sockaddr_storage));

    unsigned char pac[6] = { 0 };
    unsigned short ipv6_port = { 0 };
    if (!ftp_pasv(sa.ss_family != AF_INET, pac, &ipv6_port))
    {
      ftp_trace("PASV/EPSV failed.\n");
      sock_destroy(ftp->data);
      ftp->data = NULL;
      return -1;
    }

    socklen_t len = sizeof(struct sockaddr_in);
    if (sa.ss_family == AF_INET)
    {
      memcpy(&((struct sockaddr_in*)&sa)->sin_addr, pac, (size_t)4);
      memcpy(&((struct sockaddr_in*)&sa)->sin_port, pac+4, (size_t)2);
    }
#ifdef HAVE_IPV6
    else if (sa.ss_family == AF_INET6)
    {
      ((struct sockaddr_in6*)&sa)->sin6_port = htons(ipv6_port);
      len = sizeof(struct sockaddr_in6);
    }
#endif
    else
    {
      ftp_trace("Do not know how to handle family %d.\n", sa.ss_family);
      sock_destroy(ftp->data);
      ftp->data = NULL;
      return -1;
    }

    struct sockaddr_storage tmp;
    memcpy(&tmp, sock_remote_addr(ftp->ctrl), sizeof(struct sockaddr_storage));
    if (is_reserved((struct sockaddr*) &sa) ||
         is_multicast((struct sockaddr*) &sa)  ||
         (is_private((struct sockaddr*) &sa) != is_private((struct sockaddr*) &tmp)) ||
         (is_loopback((struct sockaddr*) &sa) != is_loopback((struct sockaddr*) &tmp)))
    {
      // Invalid address returned by PASV. Replace with address from control
      // socket.
      ftp_err(_("Address returned by PASV seems to be incorrect.\n"));
      ((struct sockaddr_in*)&sa)->sin_addr = ((struct sockaddr_in*)&tmp)->sin_addr;
    }

    if (!sock_connect_addr(ftp->data, (struct sockaddr*) &sa, len))
    {
      ftp_trace("Could not connect to address from PASV/EPSV.\n");
      perror("connect()");
      sock_destroy(ftp->data);
      ftp->data = NULL;
      return -1;
    }
  } else {
    ftp_trace("Initializing active connection.\n");

    const struct sockaddr* local = sock_local_addr(ftp->data);
    sock_listen(ftp->data, local->sa_family);

    if (local->sa_family == AF_INET)
    {
      struct sockaddr_in* tmp = (struct sockaddr_in*)local;
      unsigned char* a = (unsigned char *)&tmp->sin_addr;
      unsigned char* p = (unsigned char *)&tmp->sin_port;

      ftp_set_tmp_verbosity(vbError);
      ftp_cmd("PORT %d,%d,%d,%d,%d,%d",
          a[0], a[1], a[2], a[3], p[0], p[1]);
    }
#ifdef HAVE_IPV6
    else if (local->sa_family == AF_INET6)
    {
      char* addr = printable_address(local);

      ftp_set_tmp_verbosity(vbError);
      ftp_cmd("EPRT |2|%s|%u|", addr, ntohs(((struct sockaddr_in6*)local)->sin6_port));
      free(addr);
    }
#endif
    else
    {
      ftp_trace("Do not know how to handle family %d.\n", local->sa_family);
      sock_destroy(ftp->data);
      ftp->data = NULL;
      return -1;
    }

    if(ftp->code != ctComplete)
    {
      ftp_trace("PORT/EPRT not successful\n");
      sock_destroy(ftp->data);
      ftp->data = NULL;
      return -1;
    }
  }

  sock_throughput(ftp->data);
  return 0;
}

int ftp_type(transfer_mode_t type)
{
#ifdef HAVE_LIBSSH
	if (ftp->session)
		/* FIXME: is this relevant for ssh ? */
		return 0;
#endif

	if(type == tmCurrent)
		return 0;

	if(ftp->prev_type != type) {
		ftp_cmd("TYPE %c", type == tmAscii ? 'A' : 'I');
		if(ftp->code != ctComplete)
			return -1;
		ftp->prev_type = type;
	}
	return 0;
}

static ftp_transfer_func foo_hookf = 0;

/* abort routine originally from Cftp by Dieter Baron
 */
int ftp_abort(Socket* fp)
{
	char buf[4096];

#ifdef HAVE_LIBSSH
	if(ftp->session)
		/* FIXME: what? */
		return 0;
#endif

	if(!ftp_connected())
		return -1;

	ftp_set_close_handler();

	if (sock_check_pending(fp, false) == 1) {
		ftp_trace("There is data on the control channel, won't send ABOR\n");
		/* read remaining bytes from connection */
		while(fp && sock_read(fp, buf, sizeof(buf)) > 0)
			/* LOOP */ ;
		return 0;
	}

	ftp->ti.interrupted = true;
	ftp_err(_("Waiting for remote to finish abort...\n"));

	ftp_trace("--> telnet interrupt\n");
	if(sock_telnet_interrupt(ftp->ctrl) != 0)
		ftp_err("telnet interrupt: %s\n", strerror(errno));

	/* ftp_cmd("ABOR") won't work here,
	 * we must flush data between the ABOR command and ftp_read_reply()
	 */
	sock_krb_printf(ftp->ctrl, "ABOR");
	sock_printf(ftp->ctrl, "\r\n");
	sock_flush(ftp->ctrl);
	if(ftp_get_verbosity() == vbDebug)
		ftp_err("--> [%s] ABOR\n", ftp->url->hostname);
	else
		ftp_trace("--> [%s] ABOR\n", ftp->url->hostname);

    /* read remaining bytes from connection */
	while(fp && sock_read(fp, buf, sizeof(buf)) > 0)
		/* LOOP */ ;

	/* we expect a 426 or 226 reply here... */
	ftp_read_reply();
	if(ftp->fullcode != 426 && ftp->fullcode != 226)
		ftp_trace("Huh!? Expected a 426 or 226 reply\n");

	/* ... and a 226 or 225 reply here, respectively */
	/* FIXME: should skip this reply if prev. reply wasn't 426 or 226 ? */
	ftp_read_reply();
	if(ftp->fullcode != 226 && ftp->fullcode != 225)
		ftp_trace("Huh!? Expected a 226 or 225 reply\n");

	return -1;
}

static int wait_for_data(Socket* fp, bool wait_for_read)
{
  errno = 0;
	const int r = sock_check_pending(fp, !wait_for_read);
#if 0
	if(r < 0 && errno == EINTR && gvSigStopReceived && ftp_sigints() == 0) {
		gvSigStopReceived = false;
		r = 0;
	}
#endif
	if(r < 0) {
/*		perror("\nselect");*/
		if(errno == EINTR) {
			if(gvSighupReceived)
				return 1;
			if(gvInterrupted)
				return -1;
			if(ftp_sigints() == 0)
				/* assume it is a SIGSTOP/SIGCONT signal */
				return 0;
		}
		return -1;
	}

	if(r)
		ftp->ti.stalled = 0;
	else
		ftp->ti.stalled++;

	return r;
}

static int wait_for_input(void)
{
	int r;
	do {
		r = wait_for_data(ftp->data, true);
		if(r == -1) {
			if(errno == EINTR)
				ftp->ti.interrupted = true;
			return -1;
		}
		if(r == 0 && foo_hookf)
			foo_hookf(&ftp->ti);
	} while(r == 0);

	return 0;
}

static int wait_for_output(void)
{
	int r;
	do {
		r = wait_for_data(ftp->data, false);
		if(r == -1)
			return -1;
		if(r == 0 && foo_hookf)
			foo_hookf(&ftp->ti);
	} while(r == 0);

	return 0;
}

static int maybe_abort_in(Socket* in, FILE *out)
{
	unsigned int i = ftp_sigints();
	ftp_set_close_handler();

	ftp->ti.finished = true;

	if(ftp->ti.interrupted)
		i++;

	if(i > 0 || sock_error_in(in) || ferror(out)) {
		if(sock_error_in(in)) {
			ftp_err(_("read error: %s\n"), strerror(errno));
			ftp->ti.ioerror = true;
		}
		else if(ferror(out)) {
			ftp_err(_("write error: %s\n"), strerror(errno));
			ftp->ti.ioerror = true;
		}
		return ftp_abort(in);
	}
	return 0;
}

static int maybe_abort_out(FILE *in, Socket *out)
{
	unsigned int i = ftp_sigints();
	ftp_set_close_handler();

	ftp->ti.finished = true;

	if(ftp->ti.interrupted)
		i++;

	if(i > 0 || ferror(in) || sock_error_out(out)) {
		if(ferror(in)) {
			ftp_err(_("read error: %s\n"), strerror(errno));
			ftp->ti.ioerror = true;
		}
		else if (sock_error_out(out)) {
			ftp_err(_("write error: %s\n"), strerror(errno));
			ftp->ti.ioerror = true;
		}
		return ftp_abort(out);
	}
	return 0;
}

static int FILE_recv_binary(Socket* in, FILE *out)
{
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	sock_clearerr_in(in);
	clearerr(out);

	char* buf = xmalloc(FTP_BUFSIZ);
	while (!sock_eof(in)) {
		if(wait_for_input() != 0) {
			ftp_trace("wait_for_input() returned non-zero\n");
			break;
		}

    const ssize_t n = sock_read(in, buf, FTP_BUFSIZ);
		if (n <= 0)
			break;

		if(ftp_sigints() > 0) {
			ftp_trace("break due to sigint\n");
			break;
		}

		if(fwrite(buf, sizeof(char), n, out) != n)
			break;

		ftp->ti.size += n;

		if(foo_hookf) {
			now = time(0);
			if(now > then) {
				foo_hookf(&ftp->ti);
				then = now;
			}
		}
	}

	free(buf);
	ftp_set_close_handler();

	return maybe_abort_in(in, out);
}

static int FILE_send_binary(FILE *in, Socket *out)
{
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	clearerr(in);
	sock_clearerr_out(out);

	char* buf = xmalloc(FTP_BUFSIZ);
	while(!feof(in)) {
		ssize_t n = fread(buf, sizeof(char), FTP_BUFSIZ, in);
		if(n <= 0)
			break;

		if(ftp_sigints() > 0)
			break;

		if(wait_for_output() != 0)
			break;

    if (sock_write(out, buf, n) != n)
      break;
		ftp->ti.size += n;
		if(foo_hookf) {
			now = time(0);
			if(now > then) {
				foo_hookf(&ftp->ti);
				then = now;
			}
		}
	}
	sock_flush(out);
	free(buf);

	return maybe_abort_out(in, out);
}

static int FILE_recv_ascii(Socket* in, FILE *out)
{
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	sock_clearerr_in(in);
	clearerr(out);

  bool next = true;
  int nc = 0;
	while (true)
  {
    int c = 0;
    if (next)
    {
      c = sock_get(in);
      if (c == EOF)
        break;
    }
    else
    {
      c = nc;
      next = true;
    }

		if (ftp_sigints() > 0 || wait_for_input() != 0)
			break;

		if (c == '\n')
			ftp->ti.barelfs++;
		else if (c == '\r')
    {
			nc = sock_get(in);
			if (nc == EOF)
				break;
			if (nc != '\n')
        next = false;
      else
        c = nc;
		}
		if (fputc(c, out) == EOF)
			break;

		ftp->ti.size++;
		if(foo_hookf) {
			now = time(0);
			if(now > then) {
				foo_hookf(&ftp->ti);
				then = now;
			}
		}
	}

	return maybe_abort_in(in, out);
}

static int FILE_send_ascii(FILE* in, Socket* out)
{
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	clearerr(in);
	sock_clearerr_out(out);

  int c;
	while((c = fgetc(in)) != EOF) {
		if(ftp_sigints() > 0)
			break;

		if(wait_for_output() != 0)
			break;

		if(c == '\n') {
			if(sock_put(out, '\r') == EOF)
				break;
			ftp->ti.size++;
		}
		if(sock_put(out, c) == EOF)
			break;
		ftp->ti.size++;
		if(foo_hookf) {
			now = time(0);
			if(now > then) {
				foo_hookf(&ftp->ti);
				then = now;
			}
		}
	}

	return maybe_abort_out(in, out);
}

void reset_transfer_info(void)
{
	ftp->ti.barelfs = 0;
	ftp->ti.size = 0L;
	ftp->ti.ioerror = false;
	ftp->ti.interrupted = false;
	ftp->ti.transfer_is_put = false;
	ftp->ti.restart_size = 0L;
	ftp->ti.finished = false;
	ftp->ti.stalled = 0;
	ftp->ti.begin = true;
	gettimeofday(&ftp->ti.start_time, 0);
	if(!ftp->ti.local_name)
		ftp->ti.local_name = xstrdup("local");
	if(!ftp->ti.remote_name)
		ftp->ti.remote_name = xstrdup("remote");
}

int ftp_list(const char *cmd, const char *param, FILE *fp)
{
  if (!cmd || !fp || !ftp_connected())
    return -1;

#ifdef HAVE_LIBSSH
  if (ftp->session)
    return ssh_list(cmd, param, fp);
#endif

  reset_transfer_info();
  foo_hookf = NULL;

#if 0 /* don't care about transfer type, binary should work well... */
  ftp_type(tmAscii);
#endif

  if (ftp_init_transfer() != 0) {
    ftp_err(_("transfer initialization failed"));
    return -1;
  }

  ftp_set_tmp_verbosity(vbNone);
  if (param)
    ftp_cmd("%s %s", cmd, param);
  else
    ftp_cmd("%s", cmd);
  if (ftp->code != ctPrelim)
    return -1;

  if (!sock_accept(ftp->data, "r", ftp_is_passive())) {
    perror("accept()");
    return -1;
  }

  if (FILE_recv_ascii(ftp->data, fp) != 0)
    return -1;

  sock_destroy(ftp->data);
  ftp->data = NULL;

  ftp_read_reply();

  return ftp->code == ctComplete ? 0 : -1;
}

void transfer_finished(void)
{
	ftp->ti.finished = true;
	if(foo_hookf)
		foo_hookf(&ftp->ti);
}

static int ftp_init_receive(const char *path, transfer_mode_t mode,
							ftp_transfer_func hookf)
{
	long rp = ftp->restart_offset;
	char *e;

	ftp->restart_offset = 0L;

	foo_hookf = hookf;
	reset_transfer_info();

	if(ftp_init_transfer() != 0)
		return -1;

	ftp_type(mode);

	if(rp > 0) {
		/* fp is assumed to be fseek'd already */
		ftp_cmd("REST %ld", rp);
		if(ftp->code != ctContinue)
			return -1;
		ftp->ti.size = rp;
		ftp->ti.restart_size = rp;
	}

	ftp_cmd("RETR %s", path);
	if(ftp->code != ctPrelim)
		return -1;

	if(!sock_accept(ftp->data, "r", ftp_is_passive())) {
		ftp_err(_("data connection not accepted\n"));
		return -1;
	}

	/* try to get the total file size */
	{
		/* see if we have cached this directory/file */
		rfile *f = ftp_cache_get_file(path);
		if(f)
			ftp->ti.total_size = f->size;
		else {
			/* try to figure out file size from RETR reply
			 * Opening BINARY mode data connection for foo.mp3 (14429793 bytes)
			 *                                                  ^^^^^^^^ aha!
			 *
			 * note: this might not be the _total_ filesize if we are RESTarting
			 */
			e = strstr(ftp->reply, " bytes");
			if(e != 0) {
				while((e > ftp->reply) && isdigit((int)e[-1]))
					e--;
				ftp->ti.total_size = strtoul(e,NULL,10);
			} /* else we don't bother */
		}
	}
	return 0;
}

static int ftp_do_receive(FILE *fp,
						  transfer_mode_t mode, ftp_transfer_func hookf)
{
	int r;

	if(mode == tmBinary)
		r = FILE_recv_binary(ftp->data, fp);
	else
		r = FILE_recv_ascii(ftp->data, fp);

	sock_destroy(ftp->data);
	ftp->data = 0;

	if(r == 0) {
		transfer_finished();
		ftp_read_reply();
		ftp->ti.ioerror = (ftp->code != ctComplete);
		if(ftp->code != ctComplete) {
			ftp_trace("transfer failed\n");
			return -1;
		}
	} else
		transfer_finished();

	return (r == 0 && !ftp->ti.ioerror && !ftp->ti.interrupted) ? 0 : -1;
}

int ftp_receive(const char *path, FILE *fp,
				transfer_mode_t mode, ftp_transfer_func hookf)
{
#ifdef HAVE_LIBSSH
	if(ftp->session)
		return ssh_do_receive(path, fp, mode, hookf);
#endif

	if(ftp_init_receive(path, mode, hookf) != 0)
		return -1;

	return ftp_do_receive(fp, mode, hookf);
}

static int ftp_send(const char *path, FILE *fp, putmode_t how,
					transfer_mode_t mode, ftp_transfer_func hookf)
{
	int r;
	long rp = ftp->restart_offset;
	ftp->restart_offset = 0L;

	if(how == putUnique && !ftp->has_stou_command)
		return -1;

  if (how == putTryUnique && !ftp->has_stou_command)
    how = putNormal;

	reset_transfer_info();
	ftp->ti.transfer_is_put = true;

	if(ftp_init_transfer() != 0)
		return -1;

	ftp_type(mode);

	if(rp > 0) {
		/* fp is assumed to be fseek'd already */
		ftp_cmd("REST %ld", rp);
		if(ftp->code != ctContinue)
			return -1;
		ftp->ti.size = rp;
		ftp->ti.restart_size = rp;
	}

  ftp_set_tmp_verbosity(vbError);
  switch (how) {
  case putAppend:
    ftp_cmd("APPE %s", path);
    break;

  case putTryUnique:
  case putUnique:
    ftp_cmd("STOU %s", path);
    if (ftp->fullcode == 502 || ftp->fullcode == 504) {
      ftp->has_stou_command = false;
      if (how == putTryUnique)
        how = putNormal;
      else
        break;
    }
    else
      break;

  default:
    ftp_cmd("STOR %s", path);
    break;
  }

	if(ftp->code != ctPrelim)
		return -1;

	if(how == putUnique) {
		/* try to figure out remote filename */
		char *e = strstr(ftp->reply, " for ");
		if(e) {
			int l;
			e += 5;
			l = strlen(e);
			if(l) {
				free(ftp->ti.local_name);
				if(*e == '\'')
					ftp->ti.local_name = xstrndup(e+1, l-3);
				else
					ftp->ti.local_name = xstrndup(e, l-1);
				ftp_trace("parsed unique filename as '%s'\n",
						  ftp->ti.local_name);
			}
		}
	}

	if(!sock_accept(ftp->data, "w", ftp_is_passive())) {
		ftp_err(_("data connection not accepted\n"));
		return -1;
	}

	ftp_cache_flush_mark_for(path);

	if(mode == tmBinary)
		r = FILE_send_binary(fp, ftp->data);
	else
		r = FILE_send_ascii(fp, ftp->data);
	sock_flush(ftp->data);
	sock_destroy(ftp->data);
	ftp->data = 0;

	if(r == 0) {
		transfer_finished();
		ftp_read_reply();
		ftp->ti.ioerror = (ftp->code != ctComplete);
		if(ftp->code != ctComplete) {
			ftp_trace("transfer failed\n");
			return -1;
		}
	} else
		transfer_finished();

	return 0;
}

/* transfers SRCFILE on SRCFTP to DESTFILE on DESTFTP
 * using pasv mode on SRCFTP and port mode on DESTFTP
 *
 */
int ftp_fxpfile(Ftp *srcftp, const char *srcfile,
				Ftp *destftp, const char *destfile,
				fxpmode_t how, transfer_mode_t mode)
{
	int r;
	unsigned int old_reply_timeout;
	Ftp *thisftp;
	unsigned char addr[6];

/*	printf("FxP: %s -> %s\n", srcftp->url->hostname, destftp->url->hostname);*/

	if(srcftp == destftp) {
		ftp_err(_("FxP between same hosts\n"));
		return -1;
	}

#ifdef HAVE_LIBSSH
	if(ftp->session) {
		ftp_err("FxP with SSH not implemented\n");
		return -1;
	}
#endif

	thisftp = ftp; /* save currently active connection */

	/* setup source side */
	ftp_use(srcftp);
	ftp_type(mode);
  // TODO: IPv6 support
	if(!ftp_pasv(false, addr, NULL)) {
		ftp_use(thisftp);
		return -1;
	}
	ftp->ti.total_size = -1;

	/* setup destination side */
	ftp_use(destftp);
	ftp_type(mode);
	ftp_cmd("PORT %d,%d,%d,%d,%d,%d",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	if(ftp->code != ctComplete) {
		ftp_use(thisftp);
		return -1;
	}
	ftp->ti.total_size = -1;

	ftp_use(destftp);

	if(how == fxpResume) {
		rfile *f;

		f = ftp_get_file(destfile);
		if(f && f->size != (unsigned long long)-1)
			ftp->restart_offset = f->size;
		else {
			ftp->restart_offset = ftp_filesize(destfile);
			if(ftp->restart_offset == (unsigned long long)-1) {
				ftp_err(_("unable to get remote filesize of '%s',"
						  " unable to resume\n"),
						destfile);
				ftp->restart_offset = 0L;
			}
		}
	} else
		ftp->restart_offset = 0L;

	if(ftp->restart_offset) {
		/* RESTart on destftp */
		ftp_cmd("REST %ld", ftp->restart_offset);
		if(ftp->code != ctContinue)
			return -1;
		ftp_use(srcftp);
		ftp_cmd("REST %ld", ftp->restart_offset);
		if(ftp->code != ctContinue)
			return -1;
	}

	/* issue a STOR command on DESTFTP */
	ftp_use(destftp);
	switch(how) {
	case fxpUnique:
		if(ftp->has_stou_command) {
			ftp_cmd("STOU %s", destfile);
			if(ftp->fullcode == 502)
				ftp->has_stou_command = false;
		} else {
			ftp->code = ctError;
			ftp->fullcode = 502;
		}
		break;
	case fxpAppend:
		ftp_cmd("APPE %s", destfile);
		break;
	case fxpNormal:
	default:
		ftp_cmd("STOR %s", destfile);
		break;
	}

	ftp_cache_flush_mark_for(destfile);

	if(ftp->code != ctPrelim) {
		ftp_use(thisftp);
		return -1;
	}

	/* issue a RETR command on SRCFTP */
	ftp_use(srcftp);
	ftp_cmd("RETR %s", srcfile);
	if(ftp->code != ctPrelim) {
		ftp_use(destftp);
		ftp_abort(NULL);
		ftp_use(thisftp);
		return -1;
	}

	ftp_use(destftp);
	old_reply_timeout = ftp->reply_timeout;
	ftp_reply_timeout(0);
	ftp_read_reply();
	ftp_reply_timeout(old_reply_timeout);

	r = (ftp->code == ctComplete ? 0 : -1);

	ftp_use(srcftp);
	old_reply_timeout = ftp->reply_timeout;
	ftp_reply_timeout(0);
	ftp_read_reply();
	ftp_reply_timeout(old_reply_timeout);

	ftp_use(thisftp);

	return r;
}

int ftp_getfile(const char *infile, const char *outfile, getmode_t how,
				transfer_mode_t mode, ftp_transfer_func hookf)
{
	FILE *fp;
	int r;
	struct stat statbuf;
	long rp = 0;
	int (*close_func)(FILE *fp);

	if(stat(outfile, &statbuf) == 0) {
		if(S_ISDIR(statbuf.st_mode)) {
			ftp_err(_("%s: is a directory\n"), outfile);
			return -1;
		}
		if(!(statbuf.st_mode & S_IWRITE)) {
			ftp_err(_("%s: permission denied\n"), outfile);
			return -1;
		}
		if(how == getResume)
			ftp->restart_offset = statbuf.st_size;
	} else
		ftp->restart_offset = 0L;

	ftp->ti.total_size = -1;

	/* we need to save this, because ftp_init_receive() sets it to zero */
	rp = ftp->restart_offset;

	reset_transfer_info();
#ifdef HAVE_LIBSSH
	if (ftp->session)
	{
		/* we need to stat the remote file, so we are sure we can read it
		 * this needs to be done before we call ssh_do_receive, because by
		 * then, the file is created, and would leave a zero-size file opon
		 * failure
		 */
		sftp_attributes a = sftp_stat(ftp->sftp_session, infile);
		if(!a) {
			ftp_err(_("Unable to stat file '%s': %s\n"), infile, ssh_get_error(ftp->session));
			return -1;
		}
		sftp_attributes_free(a);
		/* FIXME: how can we check if it will be possible to transfer
		 * the specified file?
		 */
	}
	else
#endif
	if (ftp_init_receive(infile, mode, hookf) != 0)
		return -1;

	if(how == getPipe) {
		fp = popen(outfile, "w");
		close_func = pclose;
	} else {
		fp = fopen(outfile,
				   (rp > 0L || (how == getAppend)) ? "a" : "w");
		close_func = fclose;
	}
	if(!fp) {
		ftp_err("%s: %s\n", outfile, strerror(errno));
		ftp->restart_offset = 0L;
		return -1;
	}

	if(rp > 0L) {
		if(fseek(fp, rp, SEEK_SET) != 0) {
			ftp_err(_("%s: %s, transfer cancelled\n"),
					outfile, strerror(errno));
			close_func(fp);
			ftp->restart_offset = 0L;
			return -1;
		}
	}

	free(ftp->ti.remote_name);
	free(ftp->ti.local_name);
	ftp->ti.remote_name = xstrdup(infile);
	ftp->ti.local_name = xstrdup(outfile);

	foo_hookf = hookf;

#ifdef HAVE_LIBSSH
	if(ftp->session)
		r = ssh_do_receive(infile, fp, mode, hookf);
	else
#endif
		r = ftp_do_receive(fp, mode, hookf);
	close_func(fp);
	return r;
}

int ftp_putfile(const char *infile, const char *outfile, putmode_t how,
				transfer_mode_t mode, ftp_transfer_func hookf)
{
	FILE *fp;
	int r;
	struct stat statbuf;

	if(stat(infile, &statbuf) != 0) {
		perror(infile);
		return -1;
	}

	if(S_ISDIR(statbuf.st_mode)) {
		ftp_err(_("%s: is a directory\n"), infile);
		return -1;
	}

	fp = fopen(infile, "r");
	if(fp == 0) {
		perror(infile);
		return -1;
	}

	ftp->ti.total_size = statbuf.st_size;
	free(ftp->ti.remote_name);
	free(ftp->ti.local_name);
	ftp->ti.remote_name = xstrdup(infile); /* actually local file, or _target_ */
	ftp->ti.local_name = xstrdup(outfile); /* actually remote file, or _source_ */

	if(how == putResume) {
		rfile *f;

		f = ftp_get_file(outfile);
		if(f && f->size != (unsigned long long)-1)
			ftp->restart_offset = f->size;
		else {
			ftp->restart_offset = ftp_filesize(outfile);
			if(ftp->restart_offset == (unsigned long long)-1) {
				ftp_err(_("unable to get remote filesize of '%s',"
						  " unable to resume\n"),
						outfile);
				ftp->restart_offset = 0L;
			}
		}
	} else
		ftp->restart_offset = 0L;


	if(ftp->restart_offset > 0L) {
		if(fseek(fp, ftp->restart_offset, SEEK_SET) != 0) {
			ftp_err(_("%s: %s, transfer cancelled\n"),
					outfile, strerror(errno));
			fclose(fp);
			return -1;
		}
	}

	foo_hookf = hookf;

#ifdef HAVE_LIBSSH
	if(ftp->session)
		r = ssh_send(outfile, fp, how, mode, hookf);
	else
#endif
		r = ftp_send(outfile, fp, how, mode, hookf);
	fclose(fp);
	return r;
}

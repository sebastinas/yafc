/* ftpsend.c -- send/receive files and file listings
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

#include "syshdr.h"

#include "ftp.h"
#include "socket.h"
#include "xmalloc.h"
#include "ftpsigs.h"
#include "gvars.h"

static int ftp_pasv(unsigned char result[6])
{
	int pa[6];
	char *e;
	int i;

	if(!ftp->has_pasv_command) {
		ftp_err(_("Host doesn't support passive mode\n"));
		return -1;
	}
	ftp_set_tmp_verbosity(vbNone);

	/* request passive mode */
	ftp_cmd("PASV");

	if(!ftp_connected())
		return -1;

	if(ftp->code != ctComplete) {
		ftp_err(_("Unable to enter passive mode\n"));
		if(ftp->code == ctError) /* no use try it again */
			ftp->has_pasv_command = false;
		return -1;
	}

	e = ftp->reply + 4;
	while(!isdigit((int)*e))
		e++;

	if(sscanf(e, "%d,%d,%d,%d,%d,%d",
			  &pa[0], &pa[1], &pa[2], &pa[3], &pa[4], &pa[5]) != 6) {
		ftp_err(_("Error parsing PASV reply: '%s'\n"),
				ftp_getreply(false));
		return -1;
	}
	for(i=0; i<6; i++)
		result[i] = (unsigned char)(pa[i] & 0xFF);

	return 0;
}

static bool ftp_is_passive(void)
{
	if(!ftp || !ftp->url || ftp->url->pasvmode == -1)
		return gvPasvmode;
	return ftp->url->pasvmode;
}

static int ftp_init_transfer(void)
{
	struct sockaddr_in sa;
	unsigned char *a, *p;
	unsigned char pac[6];

	if(!ftp_connected())
		return -1;

	ftp->data = sock_create();
	sock_copy(ftp->data, ftp->ctrl);

	if(ftp_is_passive()) {
		if(ftp_pasv(pac) != 0)
			return -1;

		sock_getsockname(ftp->ctrl, &sa);
		memcpy(&sa.sin_addr, pac, (size_t)4);
		memcpy(&sa.sin_port, pac+4, (size_t)2);
		if(sock_connect_addr(ftp->data, &sa) == -1)
			return -1;
	} else {
		sock_listen(ftp->data);

		a = (unsigned char *)&ftp->data->local_addr.sin_addr;
		p = (unsigned char *)&ftp->data->local_addr.sin_port;

		ftp_set_tmp_verbosity(vbError);
		ftp_cmd("PORT %d,%d,%d,%d,%d,%d",
				a[0], a[1], a[2], a[3], p[0], p[1]);
		if(ftp->code != ctComplete)
			return -1;
	}

	sock_throughput(ftp->data);

	return 0;
}

int ftp_type(transfer_mode_t type)
{
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
int ftp_abort(FILE *fp)
{
	char buf[4096];
	fd_set ready;
	struct timeval poll;

	if(!ftp_connected())
		return -1;

	ftp_set_close_handler();

	poll.tv_sec = poll.tv_usec = 0;
	FD_ZERO(&ready);
	FD_SET(ftp->ctrl->handle, &ready);
	if(select(ftp->ctrl->handle+1, &ready, 0, 0, &poll) == 1) {
		ftp_trace("There is data on the control channel, won't send ABOR\n");
		/* read remaining bytes from connection */
		while(fp && fread(buf, 1, 4096, fp) > 0)
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
	while(fp && fread(buf, 1, 4096, fp) > 0)
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

static int wait_for_data(FILE *fd, bool wait_for_read)
{
	fd_set fds;
	struct timeval tv;
	int r;

	/* watch fd to see if it has input */
	FD_ZERO(&fds);
	FD_SET(fileno(fd), &fds);
	/* wait max 1 second */
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if(wait_for_read)
		r = select(fileno(fd)+1, &fds, 0, 0, &tv);
	else /* wait for write */
		r = select(fileno(fd)+1, 0, &fds, 0, &tv);
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
		r = wait_for_data(ftp->data->sin, true);
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
		r = wait_for_data(ftp->data->sout, false);
		if(r == -1)
			return -1;
		if(r == 0 && foo_hookf)
			foo_hookf(&ftp->ti);
	} while(r == 0);

	return 0;
}

static int maybe_abort(FILE *in, FILE *out)
{
	unsigned i;

	i = ftp_sigints();
	ftp_set_close_handler();

	ftp->ti.finished = true;

	if(ftp->ti.interrupted)
		i++;

	if(i > 0 || ferror(in) || ferror(out)) {
		if(ferror(in)) {
			ftp_err(_("read error: %s\n"), strerror(errno));
			ftp->ti.ioerror = true;
		}
		else if(ferror(out)) {
			ftp_err(_("write error: %s\n"), strerror(errno));
			ftp->ti.ioerror = true;
		}
		return ftp_abort(ftp->ti.transfer_is_put ? out : in);
	}
	return 0;
}

static int FILE_recv_binary(FILE *in, FILE *out)
{
	size_t n;
	char *buf;
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	clearerr(in);
	clearerr(out);

	buf = (char *)xmalloc(FTP_BUFSIZ);
	while(!feof(in)) {

		if(wait_for_input() != 0) {
			ftp_trace("wait_for_input() returned non-zero\n");
			break;
		}
#if defined(KRB4) || defined(KRB5)
		n = sec_read(fileno(in), buf, FTP_BUFSIZ);
#else
		n = fread(buf, sizeof(char), FTP_BUFSIZ, in);
#endif
		if(n <= 0)
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

	xfree(buf);
	ftp_set_close_handler();

	return maybe_abort(in, out);
}

static int FILE_send_binary(FILE *in, FILE *out)
{
	size_t n;
	char *buf;
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	clearerr(in);
	clearerr(out);

	buf = (char *)xmalloc(FTP_BUFSIZ);
	while(!feof(in)) {
		n = fread(buf, sizeof(char), FTP_BUFSIZ, in);
		if(n <= 0)
			break;

		if(ftp_sigints() > 0)
			break;

		if(wait_for_output() != 0)
			break;
#if defined(KRB4) || defined(KRB5)
		if(sec_write(fileno(out), buf, n) != n)
			break;
#else
		if(fwrite(buf, sizeof(char), n, out) != n)
			break;
#endif
		ftp->ti.size += n;
		if(foo_hookf) {
			now = time(0);
			if(now > then) {
				foo_hookf(&ftp->ti);
				then = now;
			}
		}
	}
#if defined(KRB4) || defined(KRB5)
	sec_fflush(out);
#endif

	xfree(buf);

	return maybe_abort(in, out);
}

static int krb_getc(FILE *fp)
{
#if defined(KRB4) || defined(KRB5)
	return sec_getc(fp);
#else
	return fgetc(fp);
#endif
}

static int FILE_recv_ascii(FILE *in, FILE *out)
{
	char *buf = (char *)xmalloc(FTP_BUFSIZ);
	int c;
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	clearerr(in);
	clearerr(out);

	while((c = krb_getc(in)) != EOF) {
		if(ftp_sigints() > 0)
			break;

		if(wait_for_input() != 0)
			break;

		if(c == '\n')
			ftp->ti.barelfs++;
		else if(c == '\r') {
			c = krb_getc(in);
			if(c == EOF)
				break;
			if(c != '\n') {
				ungetc(c, in);
				c = '\r';
			}
		}
		if(fputc(c, out) == EOF)
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

	xfree(buf);

	return maybe_abort(in, out);
}

static int FILE_send_ascii(FILE *in, FILE *out)
{
	char *buf = (char *)xmalloc(FTP_BUFSIZ);
	int c;
	time_t then = time(0) - 1;
	time_t now;

	ftp_set_close_handler();

	if(foo_hookf)
		foo_hookf(&ftp->ti);
	ftp->ti.begin = false;

	clearerr(in);
	clearerr(out);

	while((c = fgetc(in)) != EOF) {
		if(ftp_sigints() > 0)
			break;

		if(wait_for_output() != 0)
			break;

		if(c == '\n') {
			if(fputc('\r', out) == EOF)
				break;
			ftp->ti.size++;
		}
		if(fputc(c, out) == EOF)
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

	xfree(buf);

	return maybe_abort(in, out);
}

static void reset_transfer_info(void)
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
	if(!cmd || !fp || !ftp_connected())
		return -1;

	reset_transfer_info();
	foo_hookf = 0;

#if 0 /* don't care about transfer type, binary should work well... */
	ftp_type(tmAscii);
#endif

	if(ftp_init_transfer() != 0)
		return -1;

	if(param)
		ftp_cmd("%s %s", cmd, param);
	else
		ftp_cmd("%s", cmd);
	if(ftp->code != ctPrelim)
		return -1;

	if(sock_accept(ftp->data, "r", ftp_is_passive()) != 0) {
		perror("accept()");
		return -1;
	}

	if(FILE_recv_ascii(ftp->data->sin, fp) != 0)
		return -1;

	sock_destroy(ftp->data);

	ftp_read_reply();

	ftp->data = 0;
	return ftp->code == ctComplete ? 0 : -1;
}

static void transfer_finished(void)
{
	ftp->ti.finished = true;
	if(foo_hookf)
		foo_hookf(&ftp->ti);
}

int ftp_init_receive(const char *path, transfer_mode_t mode, ftp_transfer_func hookf)
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

	if(sock_accept(ftp->data, "r", ftp_is_passive()) != 0) {
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
				while(isdigit((int)e[-1]))
					e--;
				ftp->ti.total_size = atol(e);
			} /* else we don't bother */
		}
	}
	return 0;
}

int ftp_do_receive(FILE *fp,
				   transfer_mode_t mode, ftp_transfer_func hookf)
{
	int r;

	if(mode == tmBinary)
		r = FILE_recv_binary(ftp->data->sin, fp);
	else
		r = FILE_recv_ascii(ftp->data->sin, fp);

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
	if(ftp_init_receive(path, mode, hookf) != 0)
		return -1;

	return ftp_do_receive(fp, mode, hookf);
}

int ftp_send(const char *path, FILE *fp, putmode_t how,
			 transfer_mode_t mode, ftp_transfer_func hookf)
{
	int r;
	long rp = ftp->restart_offset;
	ftp->restart_offset = 0L;

	if(how == putUnique && !ftp->has_stou_command)
		return -1;

	foo_hookf = hookf;
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
	switch(how) {
	  case putUnique:
		ftp_cmd("STOU %s", path);
		if(ftp->fullcode == 502)
			ftp->has_stou_command = false;
		break;
	  case putAppend:
		ftp_cmd("APPE %s", path);
		break;
	  case putNormal:
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
				xfree(ftp->ti.local_name);
				if(*e == '\'')
					ftp->ti.local_name = xstrndup(e+1, l-3);
				else
					ftp->ti.local_name = xstrndup(e, l-1);
				ftp_trace("parsed unique filename as '%s'\n", ftp->ti.local_name);
			}
		}
	}

	if(sock_accept(ftp->data, "w", ftp_is_passive()) != 0) {
		ftp_err(_("data connection not accepted\n"));
		return -1;
	}

	ftp_cache_flush_mark_for(path);

	if(mode == tmBinary)
		r = FILE_send_binary(fp, ftp->data->sin);
	else
		r = FILE_send_ascii(fp, ftp->data->sin);
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

	printf("FxP: %s -> %s\n", srcftp->url->hostname, destftp->url->hostname);
	
	if(srcftp == destftp) {
		ftp_err(_("FxP between same hosts\n"));
		return -1;
	}

	thisftp = ftp; /* save currently active connection */

	/* setup source side */
	ftp_use(srcftp);
	ftp_type(mode);
	if(ftp_pasv(addr) != 0) {
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
		if(f && f->size != (unsigned long)-1)
			ftp->restart_offset = f->size;
		else {
			ftp->restart_offset = ftp_filesize(destfile);
			if(ftp->restart_offset == (unsigned long)-1) {
				ftp_err(_("unable to get remote filesize of '%s', unable to resume\n"),
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

	if(ftp->code != ctPrelim) {
		ftp_use(thisftp);
		return -1;
	}

	/* issue a RETR command on SRCFTP */
	ftp_use(srcftp);
	ftp_cmd("RETR %s", srcfile);
	if(ftp->code != ctPrelim) {
		ftp_use(destftp);
		ftp_abort(0);
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

	if(ftp_init_receive(infile, mode, hookf) != 0)
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
		return -1;
	}

	if(rp > 0L) {
		if(fseek(fp, rp, SEEK_SET) != 0) {
			ftp_err(_("%s: %s, transfer cancelled\n"), outfile, strerror(errno));
			close_func(fp);
			return -1;
		}
	}

	xfree(ftp->ti.remote_name);
	xfree(ftp->ti.local_name);
	ftp->ti.remote_name = xstrdup(infile);
	ftp->ti.local_name = xstrdup(outfile);

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
	xfree(ftp->ti.remote_name);
	xfree(ftp->ti.local_name);
	ftp->ti.remote_name = xstrdup(infile); /* actually local file, or _target_ */
	ftp->ti.local_name = xstrdup(outfile); /* actually remote file, or _source_ */

	if(how == putResume) {
		rfile *f;

		f = ftp_get_file(outfile);
		if(f && f->size != (unsigned long)-1)
			ftp->restart_offset = f->size;
		else {
			ftp->restart_offset = ftp_filesize(outfile);
			if(ftp->restart_offset == (unsigned long)-1) {
				ftp_err(_("unable to get remote filesize of '%s', unable to resume\n"),
						outfile);
				ftp->restart_offset = 0L;
			}
		}
	} else
		ftp->restart_offset = 0L;

	
	if(ftp->restart_offset > 0L) {
		if(fseek(fp, ftp->restart_offset, SEEK_SET) != 0) {
			ftp_err(_("%s: %s, transfer cancelled\n"), outfile, strerror(errno));
			fclose(fp);
			return -1;
		}
	}


	r = ftp_send(outfile, fp, how, mode, hookf);
	fclose(fp);
	return r;
}

/* Modified for use in Yafc by Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * Original copyright:
 *
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
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

#include "ssh_ftp.h"
#include "buffer.h"
#include "bufaux.h"
#include "getput.h"
#include "atomicio.h"
#include "gvars.h"

int use_ssh1 = 0;
/*char *ssh_program = _PATH_SSH_PROGRAM;*/
char *sftp_server = NULL;

#define COPY_SIZE 8192

int ssh_connect(char **args, int *in, int *out, pid_t *sshpid)
{
	int c_in, c_out;
#ifdef USE_PIPES
	int pin[2], pout[2];
	if ((pipe(pin) == -1) || (pipe(pout) == -1)) {
		ftp_err("pipe: %s\n", strerror(errno));
		return -1;
	}
	*in = pin[0];
	*out = pout[1];
	c_in = pout[0];
	c_out = pin[1];
#else /* USE_PIPES */
	int inout[2];
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, inout) == -1) {
		ftp_err("socketpair: %s\n", strerror(errno));
		return -1;
	}
	*in = *out = inout[0];
	c_in = c_out = inout[1];
#endif /* USE_PIPES */

	if ((*sshpid = fork()) == -1) {
		ftp_err("fork: %s\n", strerror(errno));
		return -1;
	} else if (*sshpid == 0) {
		if ((dup2(c_in, STDIN_FILENO) == -1) ||
		    (dup2(c_out, STDOUT_FILENO) == -1)) {
			ftp_err("dup2: %s\n", strerror(errno));
			exit(1);
		}
		close(*in);
		close(*out);
		close(c_in);
		close(c_out);

		if(ftp_get_verbosity() == vbDebug)
			fprintf(stderr, "executing '%s'...\n", gvSSHProgram);

		execv(gvSSHProgram, args);
		ftp_err("exec: %s: %s\n", gvSSHProgram, strerror(errno));
		exit(1);
	}

	close(c_in);
	close(c_out);

	return 0;
}

char **ssh_make_args(char ***args, char *add_arg)
{
	static int nargs = 0;
	char debug_buf[4096];
	int i;

	/* Init args array */
	if (*args == NULL) {
		nargs = 2;
		i = 0;
		*args = xmalloc(sizeof(*args) * nargs);
		(*args)[i++] = "ssh";
		(*args)[i++] = NULL;
	}

	/* If asked to add args, then do so and return */
	if (add_arg) {
		i = nargs++ - 1;
		*args = xrealloc(*args, sizeof(*args) * nargs);
		(*args)[i++] = add_arg;
		(*args)[i++] = NULL;
		return(NULL);
	}

	/* no subsystem if the server-spec contains a '/' */
	if (sftp_server == NULL || strchr(sftp_server, '/') == NULL)
		ssh_make_args(args, "-s");
	ssh_make_args(args, "-oForwardX11=no");
	ssh_make_args(args, "-oForwardAgent=no");
	ssh_make_args(args, use_ssh1 ? "-oProtocol=1" : "-oProtocol=2");

	/* Otherwise finish up and return the arg array */
	if (sftp_server != NULL)
		ssh_make_args(args, sftp_server);
	else
		ssh_make_args(args, "sftp");

	/* XXX: overflow - doesn't grow debug_buf */
	debug_buf[0] = '\0';
	for(i = 0; (*args)[i]; i++) {
		if(i)
			strlcat(debug_buf, " ", sizeof(debug_buf));
		strlcat(debug_buf, (*args)[i], sizeof(debug_buf));
	}
	if(ftp_get_verbosity() == vbDebug)
		ftp_err("SSH args: '%s'\n", debug_buf);
	else
		ftp_trace("SSH args: '%s'\n", debug_buf);

	return *args;
}

static const char *ssh_cmd2txt(int code)
{
	static const char *strings[] = {
		"",
		"SSH2_FXP_INIT", /* 1 */
		"",
		"SSH2_FXP_OPEN", /* 3 */
		"SSH2_FXP_CLOSE",
		"SSH2_FXP_READ",
		"SSH2_FXP_WRITE",
		"SSH2_FXP_LSTAT",
		"SSH2_FXP_FSTAT",
		"SSH2_FXP_SETSTAT",
		"SSH2_FXP_FSETSTAT", /* 10 */
		"SSH2_FXP_OPENDIR",
		"SSH2_FXP_READDIR",
		"SSH2_FXP_REMOVE",
		"SSH2_FXP_MKDIR",
		"SSH2_FXP_RMDIR",
		"SSH2_FXP_REALPATH",
		"SSH2_FXP_STAT",
		"SSH2_FXP_RENAME",
		"SSH2_FXP_READLINK",
		"SSH2_FXP_SYMLINK" /* 20 */
	};

	if(code >= 1 && code <= 20)
		return strings[code];
	return "unknown command";
}

static void ssh_print_cmd(Buffer *cmd)
{
	char code;

	code = buffer_ptr(cmd)[4];

	if(ftp_get_verbosity() == vbDebug) {
		ftp_err("--> [%s] ", ftp->url ? ftp->url->hostname : "?");
		ftp_err("%s\n", ssh_cmd2txt(code));
	} else {
		ftp_trace("--> [%s] ", ftp->url ? ftp->url->hostname : "?");
		ftp_trace("%s\n", ssh_cmd2txt(code));
	}
}

/* sends an SSH command on the control channel
 * returns 0 on success or -1 on error
 */
int ssh_cmd(Buffer *m)
{
	int mlen = buffer_len(m);
	int len;
	Buffer oqueue;

	buffer_init(&oqueue);
	buffer_put_int(&oqueue, mlen);
	buffer_append(&oqueue, buffer_ptr(m), mlen);
	buffer_consume(m, mlen);

	len = atomicio(write, ftp->ssh_out, buffer_ptr(&oqueue),
				   buffer_len(&oqueue));
	ssh_print_cmd(&oqueue);
	buffer_free(&oqueue);

	if(len <= 0) {
		ftp_err("Couldn't send packet: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static const char *ssh_reply2txt(int code)
{
	static const char *strings[] = {
		"SSH2_FXP_VERSION", /* 2 */
		"SSH2_FXP_STATUS",  /* 101 */
		"SSH2_FXP_HANDLE",  /* 102 */
		"SSH2_FXP_DATA",    /* 103 */
		"SSH2_FXP_NAME",    /* 104 */
		"SSH2_FXP_ATTRS",   /* 105 */
	};

	if(code == 2)
		return strings[0];
	else if(code >= 101 && code <= 105)
		return strings[code - 100];
	return "unknown reply";
}

static void ssh_print_reply(int code)
{
	verbose_t v = ftp_get_verbosity();

	ftp_trace("<-- [%s] %s\n",
			  ftp->url ? ftp->url->hostname : ftp->host->hostname,
			  ssh_reply2txt(code));

	if(v >= vbCommand || (ftp->code >= ctTransient && v == vbError)) {
		if(v == vbDebug)
			fprintf(stderr, "<-- [%s] %s\n",
					ftp->url ? ftp->url->hostname : ftp->host->hostname,
					ssh_reply2txt(code));
		else
			fprintf(stderr, "%s\n", ssh_reply2txt(code));
	}
}

int ssh_reply(Buffer *m)
{
	u_int len, msg_len;
	unsigned char buf[4096];
	bool reply_printed = false;

	len = atomicio(read, ftp->ssh_in, buf, 4);
	if(len == 0) {
		ftp_err("Connection closed\n");
		return -1;
	} else if(len == -1) {
		ftp_err("Couldn't read packet: %s\n", strerror(errno));
		return -1;
	}

	msg_len = GET_32BIT(buf);
	if(msg_len > 256 * 1024) {
		ftp_err("Received message too long %d\n", msg_len);
		return -1;
	}

	while(msg_len) {
		len = atomicio(read, ftp->ssh_in, buf, min(msg_len, sizeof(buf)));
		if(len == 0) {
			ftp_err("Connection closed\n");
			return -1;
		} else if (len == -1) {
			ftp_err("Couldn't read packet: %s\n", strerror(errno));
			return -1;
		}

		if(!reply_printed) {
			ssh_print_reply(buf[0]);
			reply_printed = true;
		}

		msg_len -= len;
		buffer_append(m, buf, len);
	}

	ftp->fullcode = 200;
	ftp->code = ctComplete;

	return 0;
}

/* returns SSH version on success, -1 on failure
 */
int ssh_init(void)
{
	int type, version;
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_INIT);
	buffer_put_int(&msg, SSH2_FILEXFER_VERSION);
	if(ssh_cmd(&msg) == -1)
		return -1;

	buffer_clear(&msg);

	if(ssh_reply(&msg) == -1)
		return -1;

	/* Expecting a VERSION reply */
	if ((type = buffer_get_char(&msg)) != SSH2_FXP_VERSION) {
		ftp_err("Invalid packet back from SSH2_FXP_INIT (type %d)\n", type);
		buffer_free(&msg);
		return -1;
	}
	version = buffer_get_int(&msg);

	/* Check for extensions */
	while(buffer_len(&msg) > 0) {
		char *name = buffer_get_string(&msg, NULL);
		char *value = buffer_get_string(&msg, NULL);

		ftp_err("Init extension: \"%s\"\n", name);
		xfree(name);
		xfree(value);
	}

	buffer_free(&msg);

	return version;
}

char *ssh_realpath(char *path)
{
	Buffer msg;
	u_int type, expected_id, count, id;
	char *filename, *longname;
	Attrib *a;

	expected_id = id = ftp->ssh_id++;
	ssh_send_string_request(id, SSH2_FXP_REALPATH, path, strlen(path));

	buffer_init(&msg);

	if(ssh_reply(&msg) == -1)
		return 0;
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if(id != expected_id) {
		ftp_err("ID mismatch (%d != %d)\n", id, expected_id);
		return 0;
	}

	if(type == SSH2_FXP_STATUS) {
		u_int status = buffer_get_int(&msg);

		ftp_err("Couldn't canonicalise: %s\n", fx2txt(status));
		return 0;
	} else if(type != SSH2_FXP_NAME) {
		ftp_err("Expected SSH2_FXP_NAME(%d) packet, got %s (%d)\n",
				SSH2_FXP_NAME, ssh_reply2txt(type), type);
		return 0;
	}

	count = buffer_get_int(&msg);
	if(count != 1) {
		ftp_err("Got multiple names (%d) from SSH_FXP_REALPATH\n", count);
		return 0;
	}

	filename = buffer_get_string(&msg, NULL);
	longname = buffer_get_string(&msg, NULL);
	a = decode_attrib(&msg);

	xfree(longname);

	buffer_free(&msg);

	return filename;
}

Attrib *ssh_stat(const char *path)
{
	u_int id;

	id = ftp->ssh_id++;
	ssh_send_string_request(id, SSH2_FXP_STAT, path, strlen(path));
	return ssh_get_decode_stat(id);
}

Attrib *do_lstat(const char *path)
{
	u_int id;

	id = ftp->ssh_id++;
	ssh_send_string_request(id, SSH2_FXP_LSTAT, path, strlen(path));
	return ssh_get_decode_stat(id);
}

Attrib *do_fstat(char *handle, u_int handle_len)
{
	u_int id;

	id = ftp->ssh_id++;
	ssh_send_string_request(id, SSH2_FXP_FSTAT, handle, handle_len);
	return ssh_get_decode_stat(id);
}

int ssh_setstat(const char *path, Attrib *a)
{
	u_int status, id;

	id = ftp->ssh_id++;
	ssh_send_string_attrs_request(id, SSH2_FXP_SETSTAT, path,
								  strlen(path), a);

	status = ssh_get_status(id);
	if(status != SSH2_FX_OK) {
		ftp_err("Couldn't setstat on \"%s\": %s\n", path,
				fx2txt(status));
		return -1;
	}

	return 0;
}

char *ssh_get_handle(u_int expected_id, u_int *len)
{
	Buffer msg;
	u_int type, id;
	char *handle;

	buffer_init(&msg);
	if(ssh_reply(&msg) == -1)
		return 0;
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if(id != expected_id) {
		ftp_err("ID mismatch (%d != %d)\n", id, expected_id);
		return 0;
	}
	if(type == SSH2_FXP_STATUS) {
		int status = buffer_get_int(&msg);

		ftp_err("Couldn't get handle: %s\n", fx2txt(status));
		return NULL;
	} else if (type != SSH2_FXP_HANDLE) {
		ftp_err("Expected SSH2_FXP_HANDLE(%d) packet, got %s (%d)\n",
				SSH2_FXP_HANDLE, ssh_reply2txt(type), type);
		return 0;
	}

	handle = buffer_get_string(&msg, len);
	buffer_free(&msg);

	return handle;
}

int ssh_readdir(char *path, SFTP_DIRENT ***dir)
{
	Buffer msg;
	u_int type, id, handle_len, i, expected_id, ents = 0;
	char *handle;

	id = ftp->ssh_id++;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_OPENDIR);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, path);
	if(ssh_cmd(&msg) == -1)
		return -1;

	buffer_clear(&msg);

	handle = ssh_get_handle(id, &handle_len);
	if(handle == 0)
		return -1;

	if(dir) {
		ents = 0;
		*dir = xmalloc(sizeof(**dir));
		(*dir)[0] = NULL;
	}

	while(true) {
		int count;

		id = expected_id = ftp->ssh_id++;

		buffer_clear(&msg);
		buffer_put_char(&msg, SSH2_FXP_READDIR);
		buffer_put_int(&msg, id);
		buffer_put_string(&msg, handle, handle_len);
		if(ssh_cmd(&msg) == -1)
			return -1;

		buffer_clear(&msg);

		if(ssh_reply(&msg) == -1)
			return -1;

		type = buffer_get_char(&msg);
		id = buffer_get_int(&msg);

		if(id != expected_id) {
			ftp_err("ID mismatch (%d != %d)\n", id, expected_id);
			return -1;
		}

		if(type == SSH2_FXP_STATUS) {
			int status = buffer_get_int(&msg);

			if(status == SSH2_FX_EOF) {
				break;
			} else {
				ftp_err("Couldn't read directory: %s\n", fx2txt(status));
				ssh_close(handle, handle_len);
				return status;
			}
		} else if(type != SSH2_FXP_NAME) {
			ftp_err("Expected SSH2_FXP_NAME (%d) packet, got %s (%d)\n",
					SSH2_FXP_NAME, ssh_reply2txt(type), type);
		}

		count = buffer_get_int(&msg);
		if(count == 0)
			break;
		for(i = 0; i < count; i++) {
			char *filename, *longname;
			Attrib *a;

			filename = buffer_get_string(&msg, NULL);
			longname = buffer_get_string(&msg, NULL);
			a = decode_attrib(&msg);

			if(dir) {
				*dir = xrealloc(*dir, sizeof(**dir) *
				    (ents + 2));
				(*dir)[ents] = xmalloc(sizeof(***dir));
				(*dir)[ents]->filename = xstrdup(filename);
				(*dir)[ents]->longname = xstrdup(longname);
				memcpy(&(*dir)[ents]->a, a, sizeof(*a));
				(*dir)[++ents] = NULL;
			}

			xfree(filename);
			xfree(longname);
		}
	}

	buffer_free(&msg);
	ssh_close(handle, handle_len);
	xfree(handle);

	return 0;
}

void ssh_free_dirents(SFTP_DIRENT **s)
{
	int i;

	for(i = 0; s[i]; i++) {
		xfree(s[i]->filename);
		xfree(s[i]->longname);
		xfree(s[i]);
	}
	xfree(s);
}

int ssh_close(char *handle, u_int handle_len)
{
	u_int id, status;
	Buffer msg;

	buffer_init(&msg);

	id = ftp->ssh_id++;
	buffer_put_char(&msg, SSH2_FXP_CLOSE);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, handle, handle_len);
	if(ssh_cmd(&msg) == -1)
		return -1;

	status = ssh_get_status(id);
	if(status != SSH2_FX_OK)
		ftp_err("Couldn't close file: %s\n", fx2txt(status));

	buffer_free(&msg);

	return status;
}

Attrib *ssh_get_decode_stat(u_int expected_id)
{
	Buffer msg;
	u_int type, id;
	Attrib *a;

	buffer_init(&msg);
	if(ssh_reply(&msg) == -1)
		return 0;

	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if(id != expected_id) {
		ftp_err("ID mismatch (%d != %d)\n", id, expected_id);
		return 0;
	}
	if(type == SSH2_FXP_STATUS) {
		int status = buffer_get_int(&msg);

		ftp_err("Couldn't stat remote file: %s\n", fx2txt(status));
		return 0;
	} else if (type != SSH2_FXP_ATTRS) {
		ftp_err("Expected SSH2_FXP_ATTRS (%d) packet, got %s (%d)\n",
				SSH2_FXP_ATTRS, ssh_reply2txt(type), type);
		return 0;
	}
	a = decode_attrib(&msg);
	buffer_free(&msg);

	return(a);
}

void ssh_send_string_request(u_int id, u_int code, const char *s, u_int len)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, code);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, s, len);
	ssh_cmd(&msg);
	buffer_free(&msg);
}

void ssh_send_string_attrs_request(u_int id, u_int code, const char *s,
								   u_int len, Attrib *a)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, code);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, s, len);
	encode_attrib(&msg, a);
	ssh_cmd(&msg);
	buffer_free(&msg);
}

u_int ssh_get_status(int expected_id)
{
	Buffer msg;
	u_int type, id, status;

	buffer_init(&msg);
	if(ssh_reply(&msg) == -1)
		return -1;
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id) {
		ftp_err("ID mismatch (%d != %d)\n", id, expected_id);
		return -1;
	}
	if (type != SSH2_FXP_STATUS) {
		ftp_err("Expected SSH2_FXP_STATUS(%d) packet, got %s (%d)\n",
				SSH2_FXP_STATUS, ssh_reply2txt(type), type);
		return -1;
	}

	status = buffer_get_int(&msg);
	buffer_free(&msg);

	ftp->ssh_last_status = status;

	return status;
}

int ssh_recv_binary(const char *remote_path, FILE *local_fp,
					ftp_transfer_func hookf, u_int64_t offset)
{
	u_int expected_id, handle_len, mode, type, id;
/*	u_int64_t offset;*/
	char *handle;
	Buffer msg;
	Attrib junk, *a;
	int status;
	time_t then = time(0) - 1;

	ftp_set_close_handler();

	if(hookf)
		hookf(&ftp->ti);
	ftp->ti.begin = false;

	a = ssh_stat(remote_path);
	if(a == 0)
		return -1;

	/* XXX: should we preserve set[ug]id? */
	if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		mode = S_IWRITE | (a->perm & 0777);
	else
		mode = 0666;

	if ((a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) &&
	    (a->perm & S_IFDIR)) {
		ftp_err("Cannot download a directory: %s\n", remote_path);
		return -1;
	}

	buffer_init(&msg);

	/* Send open request */
	id = ftp->ssh_id++;
	buffer_put_char(&msg, SSH2_FXP_OPEN);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, remote_path);
	buffer_put_int(&msg, SSH2_FXF_READ);
	attrib_clear(&junk); /* Send empty attributes */
	encode_attrib(&msg, &junk);
	ssh_cmd(&msg);

	handle = ssh_get_handle(id, &handle_len);
	if(handle == 0) {
		buffer_free(&msg);
		return -1;
	}

	/* Read from remote and write to local */
/*	offset = 0;*/

	while(true) {
		u_int len;
		char *data;

		id = expected_id = ftp->ssh_id++;

		buffer_clear(&msg);
		buffer_put_char(&msg, SSH2_FXP_READ);
		buffer_put_int(&msg, id);
		buffer_put_string(&msg, handle, handle_len);
		buffer_put_int64(&msg, offset);
		buffer_put_int(&msg, COPY_SIZE);
		ssh_cmd(&msg);

		buffer_clear(&msg);

		ssh_reply(&msg);
		type = buffer_get_char(&msg);
		id = buffer_get_int(&msg);
		if(id != expected_id) {
			ftp_err("ID mismatch (%d != %d)\n", id, expected_id);
			return -1;
		}
		if(type == SSH2_FXP_STATUS) {
			status = buffer_get_int(&msg);

			if (status == SSH2_FX_EOF)
				break;
			else {
				ftp_err("Couldn't read from remote "
						"file \"%s\" : %s\n", remote_path,
						fx2txt(status));
				ssh_close(handle, handle_len);
				goto done;
			}
		} else if (type != SSH2_FXP_DATA) {
			ftp_err("Expected SSH2_FXP_DATA(%d) packet, got %s (%d)\n",
			    SSH2_FXP_DATA, ssh_reply2txt(type), type);
			return -1;
		}

		data = buffer_get_string(&msg, &len);
		if (len > COPY_SIZE) {
			ftp_err("Received more data than asked for %d > %d\n",
					len, COPY_SIZE);
			return -1;
		}

		if(ftp_sigints() > 0) {
			ftp_trace("break due to sigint\n");
			break;
		}

		if(atomicio(write, fileno(local_fp), data, len) != len) {
			ftp_err("Couldn't write: %s\n", strerror(errno));
			ssh_close(handle, handle_len);
			ftp->ti.ioerror = true;
			status = -1;
			xfree(data);
			goto done;
		}

		offset += len;
		xfree(data);

		ftp->ti.size += len;

		if(hookf) {
			time_t now = time(0);
			if(now > then) {
				hookf(&ftp->ti);
				then = now;
			}
		}

	}
	status = ssh_close(handle, handle_len);
	ftp_set_close_handler();

done:
	buffer_free(&msg);
	xfree(handle);
	return status;
}

int ssh_send_binary(const char *remote_path, FILE *local_fp,
					ftp_transfer_func hookf, u_int64_t offset)
{
	u_int handle_len, id;
/*	u_int64_t offset;*/
	char *handle;
	Buffer msg;
	Attrib a;
	int status;
	time_t then = time(0) - 1;
	struct stat sb;

	ftp_set_close_handler();

	if(hookf)
		hookf(&ftp->ti);
	ftp->ti.begin = false;

	if(fstat(fileno(local_fp), &sb) == -1) {
		ftp_err("Couldn't fstat local file: %s\n", strerror(errno));
		return -1;
	}
	stat_to_attrib(&sb, &a);

	a.flags &= ~SSH2_FILEXFER_ATTR_SIZE;
	a.flags &= ~SSH2_FILEXFER_ATTR_UIDGID;
	a.perm &= 0777;
	a.flags &= ~SSH2_FILEXFER_ATTR_ACMODTIME;

	buffer_init(&msg);

	/* Send open request */
	id = ftp->ssh_id++;
	buffer_put_char(&msg, SSH2_FXP_OPEN);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, remote_path);
	buffer_put_int(&msg, SSH2_FXF_WRITE|SSH2_FXF_CREAT|SSH2_FXF_TRUNC);
	encode_attrib(&msg, &a);
	ssh_cmd(&msg);

	buffer_clear(&msg);

	handle = ssh_get_handle(id, &handle_len);
	if(handle == 0) {
		buffer_free(&msg);
		return -1;
	}

	/* Read from local and write to remote */
/*	offset = 0;*/
	while(true) {
		int len;
		char data[COPY_SIZE];

		/*
		 * Can't use atomicio here because it returns 0 on EOF, thus losing
		 * the last block of the file
		 */
		do
			len = read(fileno(local_fp), data, COPY_SIZE);
		while ((len == -1) && (errno == EINTR || errno == EAGAIN));

		if(len == -1) {
			ftp_err("Couldn't read from file: %s\n", strerror(errno));
			return -1;
		}
		if(len == 0)
			break;

		buffer_clear(&msg);
		buffer_put_char(&msg, SSH2_FXP_WRITE);
		buffer_put_int(&msg, ++id);
		buffer_put_string(&msg, handle, handle_len);
		buffer_put_int64(&msg, offset);
		buffer_put_string(&msg, data, len);
		ssh_cmd(&msg);

		status = ssh_get_status(id);
		if(status != SSH2_FX_OK) {
			ftp_err("Couldn't write to remote file \"%s\": %s\n",
					remote_path, fx2txt(status));
			ssh_close(handle, handle_len);
			goto done;
		}

		offset += len;

		ftp->ti.size += len;

		if(hookf) {
			time_t now = time(0);
			if(now > then) {
				hookf(&ftp->ti);
				then = now;
			}
		}
	}

	status = ssh_close(handle, handle_len);

done:
	xfree(handle);
	buffer_free(&msg);
	return status;
}

char *ssh_readlink(char *path)
{
	Buffer msg;
	u_int type, expected_id, count, id;
	char *filename, *longname;
	Attrib *a;

	expected_id = id = ftp->ssh_id++;
	ssh_send_string_request(id, SSH2_FXP_READLINK, path, strlen(path));

	buffer_init(&msg);

	if(ssh_reply(&msg) == -1)
		return 0;
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if(id != expected_id) {
		ftp_err("ID mismatch (%d != %d)", id, expected_id);
		return 0;
	}

	if(type == SSH2_FXP_STATUS) {
		u_int status = buffer_get_int(&msg);

		ftp_err("Couldn't readlink: %s", fx2txt(status));
		return 0;
	} else if(type != SSH2_FXP_NAME) {
		ftp_err("Expected SSH2_FXP_NAME (%d) packet, got %s (%d)",
				SSH2_FXP_NAME, ssh_reply2txt(type), type);
		return 0;
	}

	count = buffer_get_int(&msg);
	if(count != 1) {
		ftp_err("Got multiple names (%d) from SSH2_FXP_READLINK", count);
		return 0;
	}

	filename = buffer_get_string(&msg, 0);
	longname = buffer_get_string(&msg, 0);
	a = decode_attrib(&msg);

	xfree(longname);

	buffer_free(&msg);

	return filename;
}

/* modified by Martin Hedenfalk <mhe@home.se> Dec 2002
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

#include "syshdr.h"

#ifdef SECFTP

#include "ftp.h"
#include "base64.h"
#include "commands.h"


static struct
{
	enum protection_level level;
	const char *name;
} level_names[] =
{
	{prot_clear, "clear"},
	{prot_safe, "safe"},
	{prot_confidential, "confidential"},
	{prot_private, "private"}
};

const char *level_to_name(enum protection_level level)
{
	int i;

	for (i = 0; i < sizeof(level_names) / sizeof(level_names[0]); i++)
		if (level_names[i].level == level)
			return level_names[i].name;
	return _("unknown");
}

enum protection_level name_to_level(const char *name)
{
	int i;

	for (i = 0; i < sizeof(level_names) / sizeof(level_names[0]); i++)
		if (!strncasecmp(level_names[i].name, name, strlen(name)))
			return level_names[i].level;
	return (enum protection_level)-1;
}

static struct sec_client_mech *mechs[] = {
#ifdef HAVE_KRB5
	&gss_client_mech,
#endif
#ifdef HAVE_KRB4
	&krb4_client_mech,
#endif
#ifdef USE_SSL
	&ssl_client_mech,
#endif
	NULL
};

int sec_getc(FILE * F)
{
	if (ftp->sec_complete && ftp->data_prot) {
		char c;

		if (sec_read(fileno(F), &c, 1) <= 0)
			return EOF;
		return c;
	} else
		return getc(F);
}

static int block_read(int fd, void *buf, size_t len)
{
	unsigned char *p = buf;
	int b;

	while (len) {
		b = read(fd, p, len);
		if (b == 0)
			return 0;
		else if (b < 0)
			return -1;
		len -= b;
		p += b;
	}
	return p - (unsigned char *)buf;
}

static int block_write(int fd, void *buf, size_t len)
{
	unsigned char *p = buf;
	int b;

	while (len) {
		b = write(fd, p, len);
		if (b < 0)
			return -1;
		len -= b;
		p += b;
	}
	return p - (unsigned char *)buf;
}

static int sec_get_data(int fd, struct buffer *buf, int level)
{
	int len;
	int b;

	b = block_read(fd, &len, sizeof(len));
	if (b == 0)
		return 0;
	else if (b < 0)
		return -1;
	len = ntohl(len);
	buf->data = realloc(buf->data, len);
	b = block_read(fd, buf->data, len);
	if (b == 0)
		return 0;
	else if (b < 0)
		return -1;
	buf->size =
		(*ftp->mech->decode) (ftp->app_data, buf->data, len, ftp->data_prot);
	buf->index = 0;
	return 0;
}

static size_t buffer_read(struct buffer *buf, void *data, size_t len)
{
	len = min(len, buf->size - buf->index);
	memcpy(data, (char *)buf->data + buf->index, len);
	buf->index += len;
	return len;
}

static size_t buffer_write(struct buffer *buf, void *data, size_t len)
{
	if (buf->index + len > buf->size) {
		void *tmp;

		if (buf->data == NULL)
			tmp = malloc(1024);
		else
			tmp = realloc(buf->data, buf->index + len);
		if (tmp == NULL)
			return -1;
		buf->data = tmp;
		buf->size = buf->index + len;
	}
	memcpy((char *)buf->data + buf->index, data, len);
	buf->index += len;
	return len;
}

int sec_read(int fd, void *data, int length)
{
	size_t len;
	int rx = 0;

	if (ftp->sec_complete == 0 || ftp->data_prot == 0)
		return read(fd, data, length);

	if (ftp->in_buffer.eof_flag) {
		ftp->in_buffer.eof_flag = 0;
		return 0;
	}

	len = buffer_read(&ftp->in_buffer, data, length);
	length -= len;
	rx += len;
	data = (char *)data + len;

	while (length) {
		if (sec_get_data(fd, &ftp->in_buffer, ftp->data_prot) < 0)
			return -1;
		if (ftp->in_buffer.size == 0) {
			if (rx)
				ftp->in_buffer.eof_flag = 1;
			return rx;
		}
		len = buffer_read(&ftp->in_buffer, data, length);
		length -= len;
		rx += len;
		data = (char *)data + len;
	}
	return rx;
}

static int sec_send(int fd, char *from, int length)
{
	int bytes;
	void *buf;

	bytes =
		(*ftp->mech->encode) (ftp->app_data, from, length, ftp->data_prot,
							  &buf);
	bytes = htonl(bytes);
	block_write(fd, &bytes, sizeof(bytes));
	block_write(fd, buf, ntohl(bytes));
	free(buf);
	return length;
}

int sec_fflush(FILE * F)
{
	if (ftp->data_prot != prot_clear) {
		if (ftp->out_buffer.index > 0) {
			sec_write(fileno(F), ftp->out_buffer.data, ftp->out_buffer.index);
			ftp->out_buffer.index = 0;
		}
		sec_send(fileno(F), NULL, 0);
	}
	fflush(F);
	return 0;
}

int sec_write(int fd, char *data, int length)
{
	int len = ftp->buffer_size;
	int tx = 0;

	if (ftp->data_prot == prot_clear)
		return write(fd, data, length);

	len -= (*ftp->mech->overhead) (ftp->app_data, ftp->data_prot, len);
	while (length) {
		if (length < len)
			len = length;
		sec_send(fd, data, len);
		length -= len;
		data += len;
		tx += len;
	}
	return tx;
}

int sec_vfprintf2(FILE * f, const char *fmt, va_list ap)
{
	char *buf;
	int ret;

	if (ftp->data_prot == prot_clear)
		return vfprintf(f, fmt, ap);
	else {
		vasprintf(&buf, fmt, ap);
		ret = buffer_write(&ftp->out_buffer, buf, strlen(buf));
		free(buf);
		return ret;
	}
}

int sec_fprintf2(FILE * f, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = sec_vfprintf2(f, fmt, ap);
	va_end(ap);
	return ret;
}

int sec_putc(int c, FILE * F)
{
	char ch = c;

	if (ftp->data_prot == prot_clear)
		return putc(c, F);

	buffer_write(&ftp->out_buffer, &ch, 1);
	if (c == '\n' || ftp->out_buffer.index >= 1024 /* XXX */ ) {
		sec_write(fileno(F), ftp->out_buffer.data, ftp->out_buffer.index);
		ftp->out_buffer.index = 0;
	}
	return c;
}

int sec_read_msg(char *s, int level)
{
	int len;
	char *buf;
	int code;

	buf = malloc(strlen(s));
	len = base64_decode(s + 4, buf);	/* XXX */

	len = (*ftp->mech->decode) (ftp->app_data, buf, len, level);
	if (len < 0)
		return -1;

	buf[len] = '\0';

	if (buf[3] == '-')
		code = 0;
	else
		sscanf(buf, "%d", &code);
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';
	strcpy(s, buf);
	free(buf);
	return code;
}

int sec_vfprintf(FILE * f, const char *fmt, va_list ap)
{
	char *buf;
	void *enc;
	int len;

	if (!ftp->sec_complete)
		return vfprintf(f, fmt, ap);

	vasprintf(&buf, fmt, ap);
	len =
		(*ftp->mech->encode) (ftp->app_data, buf, strlen(buf),
							  ftp->command_prot, &enc);
	free(buf);
	if (len < 0) {
		printf(_("Failed to encode command.\n"));
		return -1;
	}
	if (base64_encode(enc, len, &buf) < 0) {
		printf(_("Out of memory base64-encoding.\n"));
		return -1;
	}
	if (ftp->command_prot == prot_safe)
		fprintf(f, "MIC %s", buf);
	else if (ftp->command_prot == prot_private)
		fprintf(f, "ENC %s", buf);
	else if (ftp->command_prot == prot_confidential)
		fprintf(f, "CONF %s", buf);
	free(buf);
	return 0;
}

int sec_fprintf(FILE * f, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = sec_vfprintf(f, fmt, ap);
	va_end(ap);
	return ret;
}

/* end common stuff */

void sec_status(void)
{
	if (ftp->sec_complete) {
		printf(_("Using %s for authentication.\n"), ftp->mech->name);
		printf(_("Using %s command channel.\n"),
			   level_to_name(ftp->command_prot));
		printf(_("Using %s data channel.\n"), level_to_name(ftp->data_prot));
		if (ftp->buffer_size > 0)
			printf(_("Protection buffer size: %lu.\n"),
				   (unsigned long)ftp->buffer_size);
	} else {
		printf(_("Not using any security mechanism.\n"));
	}
}

static int sec_prot_internal(int level)
{
	int ret;
	char *p;
	unsigned int s = 1048576;

	ftp_set_tmp_verbosity(vbError);

	if (!ftp->sec_complete) {
		ftp_err(_("No security data exchange has taken place.\n"));
		return -1;
	}

	if (level) {
		ret = ftp_cmd("PBSZ %u", s);
		if (ftp->code != ctComplete) {
			ftp_err(_("Failed to set protection buffer size.\n"));
			return -1;
		}
		ftp->buffer_size = s;
		p = strstr(ftp->reply, "PBSZ=");
		if (p)
			sscanf(p, "PBSZ=%u", &s);
		if (s < ftp->buffer_size)
			ftp->buffer_size = s;
	}

	ret = ftp_cmd("PROT %c", level["CSEP"]);	/* XXX :-) */
	if (ftp->code != ctComplete) {
		ftp_err(_("Failed to set protection level.\n"));
		return -1;
	}

	ftp->data_prot = (enum protection_level)level;
	url_setprotlevel(ftp->url, level_to_name(ftp->data_prot));
	return 0;
}

enum protection_level set_command_prot(enum protection_level level)
{
	enum protection_level old = ftp->command_prot;

	ftp->command_prot = level;
	return old;
}

void cmd_prot(int argc, char **argv)
{
	int level = -1;

	OPT_HELP
		("Set Kerberos protection level for command or data channel.  Usage:\n"
		 "  idle [options] [command|data] level\n" "Options:\n"
		 "  -h, --help    show this help\n"
		 "level should be one of the following:\n" "  clear\n" "  safe\n"
		 "  confidential\n" "  private\n");

	minargs(optind);
	maxargs(optind + 1);

	if (!ftp->sec_complete) {
		ftp_err(_("No security data exchange has taken place\n"));
		return;
	}
	level = name_to_level(argv[argc - 1]);

	if (level == -1) {
		ftp_err(_("Unrecognized protection level %s\n"), argv[argc - 1]);
		return;
	}

	if ((*ftp->mech->check_prot) (ftp->app_data, level)) {
		ftp_err(_("%s does not implement %s protection\n"),
			   ftp->mech->name, level_to_name(level));
		return;
	}

	if (argc == optind + 1 || strncasecmp(argv[optind], "data",
										  strlen(argv[optind])) == 0) {
		if (sec_prot_internal(level) < 0) {
			return;
		}
	} else if (strncasecmp(argv[optind], "command", strlen(argv[optind])) ==
			   0) set_command_prot(level);
	else {
		ftp_err(_("Syntax error, try %s --help for more information\n"),
			   argv[0]);
	}

	return;
}

static enum protection_level request_data_prot;

void sec_set_protection_level(void)
{
	if (ftp->sec_complete && ftp->data_prot != request_data_prot)
		sec_prot_internal(request_data_prot);
}

int sec_request_prot(char *level)
{
	int l = name_to_level(level);

	if (l == -1)
		return -1;
	request_data_prot = (enum protection_level)l;
	return 0;
}

static struct sec_client_mech **find_mech(const char *name)
{
	struct sec_client_mech **m;

	for(m = mechs; *m && (*m)->name; m++) {
		if(strcmp(name, (*m)->name) == 0)
			return m;
	}
	return 0;
}

/* return values:
 * 0 - success
 * 1 - failed, try another mechanism
 * -1 - failed, security extensions not supported
 */
int sec_login(char *host, const char *mech_to_try)
{
	int ret;
	struct sec_client_mech **m;
	void *tmp;

	/* shut up all messages this will produce (they
	 are usually not very user friendly) */
	ftp_set_tmp_verbosity(vbError);

	if(!mech_to_try || strcasecmp(mech_to_try, "none") == 0)
		return 0;

	m = find_mech(mech_to_try);
	if(!m)
		return 1;

	tmp = realloc(ftp->app_data, (*m)->size);
	if (tmp == NULL) {
		ftp_err(_("realloc %u failed"), (*m)->size);
		return -1;
	}
	ftp->app_data = tmp;

	if ((*m)->init && (*(*m)->init) (ftp->app_data) != 0) {
		printf(_("Skipping %s...\n"), (*m)->name);
		return 1;
	}
	printf(_("Trying %s...\n"), (*m)->name);
	ret = ftp_cmd("AUTH %s", (*m)->name);
	if (ftp->code != ctContinue) {
		if (ret == 504) {
			printf(_("%s is not supported by the server.\n"), (*m)->name);
		} else if (ret == 534) {
			printf(_("%s rejected as security mechanism.\n"), (*m)->name);
		} else if (ftp->code == ctError) {
			printf(_("The server doesn't support the FTP "
					 "security extensions.\n"));
			return -1;
		}
		return 1;
	}

	ret = (*(*m)->auth) (ftp->app_data, host);

	if (ret == AUTH_CONTINUE)
		return 1;
	else if (ret != AUTH_OK) {
		/* mechanism is supposed to output error string */
		return -1;
	}
	ftp->mech = *m;
	ftp->sec_complete = 1;
	ftp->command_prot = prot_safe;

	return *m == NULL;
}

void sec_end(void)
{
	if (ftp->mech != NULL && ftp->app_data != NULL) {
		if (ftp->mech->end)
			(*ftp->mech->end) (ftp->app_data);
		memset(ftp->app_data, 0, ftp->mech->size);
		free(ftp->app_data);
		ftp->app_data = NULL;
	}
	ftp->sec_complete = false;
	ftp->data_prot = (enum protection_level)0;
}

#endif /* SECFTP */

/*
 * strq.c -- string functions, handles quoted text
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
#include "strq.h"
#include "bashline.h"

/* Strip whitespace from the start and end of STRING.  Returns a pointer
 * into STRING.
 */
char *strip_blanks(char *str)
{
    char *s, *t;

    for(s=str;isspace((int)*s); s++)
		;

    if(*s == 0)
		return(s);

    t = s + strlen (s) - 1;
    while (t > s && isspace((int)*t) && t[-1]!='\\')
		t--;
    *++t = '\0';

    return s;
}

/* replaces control characters in STR with a '?' */
void fix_unprintable(char *str)
{
	while(str && *str) {
		if(iscntrl((int)*str))
			*str='?';
		str++;
	}
}

char *stripslash(char *p)
{
	char *e;

	if(!p || !*p)
		return p;
	e = strchr(p, 0);
	if(!e)
		return p;
	e--;
	if(*e == '/') {
		if(e != p) /* root directory */
			*e = 0;
	}
	return p;
}

const char *base_name_ptr(const char *path)
{
	char *e = strrchr(path, '/');
	if(e)
		return e+1;
	return path;
}

char *base_dir_xptr(const char *path)
{
	char *e;

	if(!path)
		return 0;

	e = strrchr(path, '/');
	if(!e)
		return 0;
	if(e == path) /* root directory */
		return xstrdup("/");
	return xstrndup(path, e-path);
}

void strpush(char *s, int n)
{
	int a;

	for(a=strlen(s);a>=0;a--)
		s[n+a] = s[a];
}

void strpull(char *s, int n)
{
	int l = strlen(s);
	if(n > l)
		n = strlen(s);
	memmove(s, s+n, l-n+1);
}

/* returns number of C in STR */
int strqnchr(const char *str, char c)
{
	int n=0;
	bool dq;

	if(!str)
		return 0;

	while(*str) {
		/* string is quoted using double or single quotes (" or ') */
		if((dq = *str == '"') == true || *str == '\'') {
			str++;
			while(*str) {
				/* Allow backslash-quoted characters to pass through unscathed. */
				if(*str == '\\') {
					str++;
					if(!*str) break;
				} else if((dq && *str == '\"') || (!dq && *str == '\'')) {
					str++;
					break;
				}
				str++;
			}
		}
		if(!*str)
			break;
		if(*str == '\\') {
			str++;
			if(!*str)
				break;
			str++;
		}
		n += (*str++ == c);
	}
	return n;
}

char *strqchr(char *s, int ch)
{
	while(s && *s) {
		if(*s == '\\') {
			s++;
			if(!s) break;
		} else if(*s == (char)ch)
			return s;
		s++;
	}
	return 0;
}

/* quoted strsep */
char *strqsep(char **s, char delim)
{
	char *e = 0;
	char *b;
	bool inquote = false;
	char quote_char='?';

	if(!s) return 0;
	e=*s;
	if(!e) return 0;

	while(*e && *e == delim)
		e++;
	if(!*e)
		return 0;
	b=e;
	while(*e) {
		if(*e=='\\') {
			e++;
			if(!*e) break;
			e++;
			continue;
		}
		if(*e == '\"') {
			if(inquote) {
				if(quote_char=='\"')
					inquote=false;
			} else {
				inquote=true;
				quote_char = '\"';
			}
		}
		if(*e == '\'') {
			if(inquote) {
				if(quote_char=='\'')
					inquote=false;
			} else {
				inquote=true;
				quote_char = '\'';
			}
		}
		else if(*e == delim && !inquote)
			break;
		e++;
	}
	*s = e + (*e ? 1 : 0);
	*e = '\0';

	return b;
}

void unquote_escapes(char *str)
{
	char *e = str;
	char tmp[3];

	if(!str)
		return;

	while(*str) {
		if(*str=='\\') {
			str++;
			if(!*str)
				break;
			if((*str=='x' || *str=='X') && isxdigit((int)*(str+1))) {
				str++;
				tmp[2]='\0';
				tmp[0]=*str++;
				if(isxdigit((int)*str))
					tmp[1]=*str;
				else tmp[1]='\0';
				*e++ = strtol(tmp, 0, 0x10);
			} else if(*str=='t')
				*e++ = '\t';
			else if(*str=='n')
				*e++ = '\n';
			else if(*str=='r')
				*e++ = '\r';
			else if(*str=='b')
				*e++ = '\b';
			else if(*str=='e')  /* escape */
				*e++ = '\x1B';
			else *e++ = *str;
		} else
			*e++ = *str;
		str++;
	}
	*e='\0';
}

void unquote(char *str)
{
	char *dq = bash_dequote_filename(str, 0);
	strcpy(str, dq);
	free(dq);
}

char *path_absolute(const char *path, const char *curdir, const char *homedir)
{
	char *p;

	if(!path)
		return xstrdup(curdir);

	if(path[0] == '~' && homedir)
		p = tilde_expand_home(path, homedir);
	else if(path[0] != '/' && path[1] != ':' && path[2] != '\\') {
		if(strncmp(path, "./", 2) == 0)
			asprintf(&p, "%s%s", curdir, path+1);
		else
			asprintf(&p, "%s/%s", curdir, path);
	}
	else
		p = xstrdup(path);

	path_collapse(p);
	return p;
}

static bool collapse_one(char *path)
{
	unsigned sp = 0, osp; /* slash pointer */
	unsigned i = 0;
	bool abspath;

	if(strchr(path, '/') == 0)
		return false;

	abspath = (path[0] == '/');

	/* ./foo/ -> foo/ */
	if(path[0]=='.' && path[1]=='/' && path[2]!='\0') {
		strpull(path, 2);
		return true;
	}

#if 0
	if(strlen(path) >= 2 && strncmp(path, "..", strlen(path)-2) == 0)
	   *this += '/';
#endif

	while(path[i]) {
		if(path[i] == '/') {
			osp = sp;
			sp = i++;
			if(!path[i])
				return false;
			if(path[i]=='.' && (path[i+1]=='/' || path[i+1]=='\0')) {
#if 0
				if(path[i+1]=='\0' && i==1) /* special case: "/." -> "/" */
					strcpy(path, "/");
				else
					strpull(path+i, 2);
#endif
				strpull(path+i, 2);
				return true;
			}
			else if(path[i]=='.' && path[i+1]=='.'
					&& (path[i+2]=='/' || path[i+2]=='\0'))
			{
				int x = 0;

				if(path[i+2]=='\0' && i==1) { /* special case: "/.." -> "/" */
					strcpy(path, "/");
					return true;
				}
				if(osp == 0 && path[osp] != '/')
					x = 1;
				/* "bar/foo/../fu" -> "bar/fu" */
				strpull(path+osp, sp+3+x-osp);
				if(path[0] == 0) {
					if(abspath)
						strcpy(path, "/");
					else
						strcpy(path, "./");
				}
				return true;
			}
		}
		i++;
	}
	return false;
}

/* collapses . and .. directories */
char *path_collapse(char *path)
{
	unsigned i;
	char drive = 0;

	if(path[0] && path[1] == ':' && path[2] == '\\') {
		/* DOS path */
		/* save and remove drive from path */
		drive = path[0];
		strpull(path, 2);
		path_dos2unix(path);
	}

	i = 0;
	/* remove "//"'s */
	while(path[i]) {
		if(path[i] == '/' && path[i+1]=='/') {
			strpull(path+i, 1);
			continue;
		}
		i++;
	}

	while(collapse_one(path))
		/* do nothing */ ;

	if(drive != 0) { /* restore DOS path */
		strpush(path, 2);
		path[0] = drive;
		path[1] = ':';
#if 0 /* they should understand '/' as well as '\' */
		for(i=0;path[i];i++) {
			if(path[i] == '/')
				path[i] = '\\';
		}
#endif
	}

	return path;
}

/* change DOS dirseparator to UNIX dirseparator, \ -> / */
char *path_dos2unix(char *path)
{
	int i;
	for(i = 0; path[i]; i++) {
		if(path[i] == '\\')
			path[i] = '/';
	}
	return path;
}

int str2bool(const char *e)
{
	if(strcasecmp(e, "on")==0) return true;
	else if(strcasecmp(e, "true")==0) return true;
	else if(strcasecmp(e, "yes")==0) return true;
	else if(strcmp(e, "1")==0) return true;
	else if(strcasecmp(e, "off")==0) return false;
	else if(strcasecmp(e, "false")==0) return false;
	else if(strcasecmp(e, "no")==0) return false;
	else if(strcmp(e, "0")==0) return false;

	return -1;
}

/* returns string with ~/foo expanded to /home/username/foo
 * can handle ~user/foo
 * returned string should be free'd
 */
char *tilde_expand_home(const char *str, const char *home)
{
	char *full, *s;

	if(home && (strcmp(str, "~") == 0))
		full = xstrdup(home);
	else if(home && (strncmp(str, "~/", 2) == 0))
		asprintf(&full, "%s%s", home, str + 1);
	else if ((str[0] == '~') && ((s = strchr(str, '/')) ||
				(s = str+strlen(str)))) {
		char *user;
		struct passwd *pass;

		user = xmalloc(s - str);
		memcpy(user, str+1, s-str-1);
		user[s-str-1] = 0;

		if ((pass = getpwnam(user)) || (!strlen(user) &&
					(pass = getpwuid(getuid()))))
			asprintf(&full, "%s%s", pass->pw_dir, s);
		else
			full = xstrdup(str);
		free(user);
	} else
		full = xstrdup(str);

	return full;
}

/* encodes special characters found in STR
 * any extra characters to be encoded should be passed in XTRA
 * returns a new string, or 0 if error
 */
char *encode_rfc1738(const char *str, const char *xtra)
{
	char *xstr, *xp;
	unsigned char c;
	static const char *special = " <>\"#%{}|\\^~[]`";
	static const char *hex = "0123456789ABCDEF";

	if(!str)
		return 0;

	xp = xstr = (char *)xmalloc(strlen(str)*3 + 1); /* worst case */

	for(; *str; str++) {
		c = *str;

		if(c >= 0x80 || c <= 0x1F || c == 0x7F
		   || strchr(special, c) != 0 || (xtra && strchr(xtra, c) != 0))
		{
			*xp++ = '%';
			*xp++ = hex[c >> 4];
			*xp++ = hex[c & 0x0F];
		} else
			*xp++ = c;
	}
	*xp = 0;

	return xstr;
}

char *decode_rfc1738(const char *str)
{
	char *xstr, *xp;
	char tmp[3];

	if(!str)
		return 0;

	xp = xstr = (char *)xmalloc(strlen(str) + 1);

	for(; *str; str++) {
		if(*str == '%' && isxdigit((int)*(str+1)) && isxdigit((int)*(str+2))) {
			tmp[0] = *++str;
			tmp[1] = *++str;
			tmp[2] = 0;
			*xp++ = (char)strtoul(tmp, 0, 16);
		} else
			*xp++ = *str;
	}

	return xstr;
}

char *encode_url_directory(const char *str)
{
	char *e;
	e = encode_rfc1738(str, ";");
	if(e && e[0] == '/') {
		char *tmp = (char *)xmalloc(strlen(e) + 3);
		strcpy(tmp, "%2F");
		strcat(tmp, e+1);
		free(e);
		return tmp;
	} else
		return e;
}

void strip_trailing_chars(char *str, const char *chrs)
{
	char *endstr;

	if(!str)
		return;

	endstr = strchr(str, 0) - 1;

	while(endstr >= str && strchr(chrs, *endstr)) {
		*endstr = 0;
		endstr--;
	}
}

/* login.c -- connect and login
 * 
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998, 1999, 2000 Martin Hedenfalk
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
#include "gvars.h"
#include "commands.h"
#include "input.h"
#include "login.h"

/* in bookmark.c */
void auto_create_bookmark(void);

/* in rc.c */
url_t *get_autologin_url(const char *host);

/* in main.c */
void init_ftp(void);

/* in tag.c */
void load_taglist(bool showerr, bool always_autoload,
				  const char *alt_filename);

static void print_user_syntax(void)
{
	printf(_("Send new user information.  Usage:\n"
			 "  user [options] [username]\n"
			 "Options:\n"
			 "  -m, --mechanism=MECH       try MECH as security mechanism(s) when logging in\n"
			 "      --help             display this help\n"));
}

void cmd_user(int argc, char **argv)
{
	char *u, *p;
	struct option longopts[] = {
		{"mechanism", required_argument, 0, 'm'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0},
	};
	int c;
	char *mech = 0;

	optind = 0;
	while((c=getopt_long(argc, argv, "m:", longopts, 0)) != EOF) {
		switch(c) {
		  case 'h':
			print_user_syntax();
			return;
		  case 'm':
			  mech = xstrdup(optarg);
			break;
		  case '?':
			return;
		}
	}

	maxargs(1);

	u = xstrdup(ftp->url->username);
	p = xstrdup(ftp->url->password);

	if(argc >= 2)
		url_setusername(ftp->url, argv[1]);
	else
		url_setusername(ftp->url, 0);
	url_setpassword(ftp->url, 0);

	if(ftp_login(u, gvAnonPasswd) == 2) {
		url_setusername(ftp->url, u);
		url_setpassword(ftp->url, p);
	}

	xfree(u);
	xfree(p);
}

void yafc_open(const char *host, unsigned int opt, const char *mech)
{
	url_t *url;
	url_t *xurl;
	int i;

	url = url_init(host);
	if(!url->hostname) {
		fprintf(stderr, _("Connect to what!? Need a hostname!\n"));
		return;
	}

	xurl = test(opt, OP_NOALIAS) ? 0 : get_autologin_url(url->hostname);
	if(xurl == (url_t *)-1) {
		fprintf(stderr, _("Ambiguous alias/hostname, use 'open --noalias' to skip bookmark alias lookup\n"));
		return;
	}

#if 0
	if(ftp_connected()) {
		if(gvPromptOnDisconnect) {
			int a = ask(ASKYES|ASKNO, ASKYES,
						_("Disconnect from %s?"), ftp->host->hostname);
			if(a == ASKNO) {
				url_destroy(url);
				return;
			}
		}
		auto_create_bookmark();
		ftp_quit();
		gvars_reset();
	}
#else
	if(ftp_connected()) {
		listitem *li;
		bool found_unconnected = false;
		
		/* first see if there are any Ftp not connected in the list */
		for(li=gvFtpList->first; li; li=li->next) {
			if(!((Ftp *)li->data)->connected
			   || !sock_connected(((Ftp *)li->data)->ctrl))
			{
				gvCurrentFtp = li;
				found_unconnected = true;
				break;
			}
		}

		if(!found_unconnected) {
			/* no unconnected found, create a new */
			list_additem(gvFtpList, ftp_create());
			gvCurrentFtp = gvFtpList->last;
		}

		ftp_use((Ftp *)gvCurrentFtp->data);
		ftp_initsigs();
		init_ftp();
	}
#endif

	if(test(opt, OP_ANON)) {
		url_setusername(url, "anonymous");
		if(!url->password)
			url_setpassword(url, gvAnonPasswd);
	}

	if(mech)
		url_setmech(url, mech);

	if(xurl) {
		url_sethostname(url, xurl->hostname);
		url_setalias(url, xurl->alias);

		if(!test(opt, OP_NOAUTO)) {
			url_setprotlevel(url, xurl->protlevel);
			if(!url->directory)
				url_setdirectory(url, xurl->directory);
			if(!url->username)
				url_setusername(url, xurl->username);
			if(!url->password && url->username && xurl->username
			   && strcmp(url->username, xurl->username) == 0)
				url_setpassword(url, xurl->password);
			if(url->port == -1)
				url_setport(url, xurl->port);
			if(!url->mech)
				url->mech = list_clone(xurl->mech, (listclonefunc)xstrdup);
			url->noproxy = xurl->noproxy;
		}
	}
	
	if(test(opt, OP_NOPROXY))
		url->noproxy = true;

	for(i=0; gvConnectAttempts == -1 || i<gvConnectAttempts; i++) {
		int r = ftp_open_url(url);
		if(r == 0)
			r = ftp_login(xurl && xurl->username ? xurl->username : gvUsername,
						  gvAnonPasswd);

		if(r == 0)
			break;

		ftp_close();
		if(r == -1)
			break;

		if(gvConnectAttempts != -1 && i+1 < gvConnectAttempts) {
			ftp_set_close_handler();
			fprintf(stderr, _("Sleeping %u seconds before connecting again (attempt #%d)...\n"),
				   gvConnectWaitTime, i+2);
			sleep(gvConnectWaitTime);
			if(ftp_sigints()) {
				fprintf(stderr, _("connection aborted\n"));
				break;
			}
		}
	}
	url_destroy(xurl);
	url_destroy(url);
	if(!ftp_connected())
		cmd_close(0, 0);
	else if(ftp_connected() && gvStartupSyst) {
		cmd_system(0, 0);
		load_taglist(false, false, 0);
	}
}

static void print_open_syntax(void)
{
	printf(_("Connect and login to remote host.  Usage:\n"
			 "  open [options] [ftp://][user[:password]@]hostname[:port][/directory] ...\n"
			 "Options:\n"
			 "  -a, --anon                 try to login anonymously\n"
			 "  -u, --noauto               disable autologin\n"
			 "  -U, --noalias              disable bookmark alias lookup and abbreviation\n"
			 "  -m, --mechanism=MECH       try MECH as security mechanism(s) when logging in\n"
			 "  -p, --noproxy              don't connect via proxy\n"
			 "      --help                 display this help and exit\n"));
}

void cmd_open(int argc, char **argv)
{
	int c;
	struct option longopts[] = {
		{"anon", no_argument, 0, 'a'},
		{"noauto", no_argument, 0, 'u'},
		{"noalias", no_argument, 0, 'U'},
		{"help", no_argument, 0, 'h'},
		{"mechanism", required_argument, 0, 'm'},
		{"noproxy", no_argument, 0, 'p'},
		{0, 0, 0, 0},
	};
	unsigned int opt = 0;
	char *mech = 0;
	int i;

	if(!gvAutologin)
		opt |= OP_NOAUTO;

	optind = 0; /* force getopt() to re-initialize */
	while((c = getopt_long(argc, argv, "auUkKp", longopts, 0)) != EOF) {
		switch(c) {
		  case 'a':
			opt |= OP_ANON;
			break;
		  case 'u':
			/* disable autologin but not alias lookup */
			opt |= OP_NOAUTO;
			break;
		  case 'U':
			/* disable alias lookup */
			opt |= OP_NOALIAS;
			break;
		  case 'm':
			  mech = xstrdup(optarg);
			break;
		  case 'p':
			opt |= OP_NOPROXY;
			break;
		  case 'h':
			print_open_syntax();
			return;
		  case '?':
			return;
		}
	}

	minargs(optind);

	for(i=optind; i<argc; i++)
		yafc_open(argv[i], opt, mech);
}

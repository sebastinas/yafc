/* put.c -- store file(s) on remote
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
#include "shortpath.h"
#include "gvars.h"
#include "put.h"
#include "strq.h"
#include "transfer.h"
#include "input.h"
#include "commands.h"
#include "lglob.h"
#include "utils.h"

#ifdef HAVE_REGEX_H
# include <regex.h>
#endif

static bool put_batch = false;
static bool put_owbatch = false;
static bool put_delbatch = false;
static bool put_quit = false;

static char *put_glob_mask = 0;
static char *put_dir_glob_mask = 0;
#ifdef HAVE_REGEX
static regex_t put_rx_mask;
static bool put_rx_mask_set = false;
static regex_t put_dir_rx_mask;
static bool put_dir_rx_mask_set = false;
#endif

static void print_put_syntax(void)
{
	printf(_("Send files to remote.  Usage:\n"
			 "  put [options] file(s) (can include wildcards)\n"
			 "Options:\n"
			 "  -a, --append         append if destination file exists\n"
			 "  -D, --delete-after   delete local file after successful transfer\n"
			 "      --dir-mask=GLOB  enter only directories matching GLOB pattern\n"
			 "      --dir-rx-mask=REGEXP\n"
			 "                       enter only directories matching REGEXP pattern\n"
			 "  -f, --force          overwrite existing destinations, never prompt\n"
			 "  -H, --nohup          transfer files in background (nohup mode), quits yafc\n"
			 "  -i, --interactive    prompt before transferring each file\n"
			 "  -L, --logfile=FILE   specify other logfile used by --nohup\n"
			 "  -m, --mask=GLOB      put only files matching GLOB pattern\n"
			 "  -M, --rx-mask=REGEXP put only files matching REGEXP pattern\n"
			 "  -n, --newer          transfer file if local is newer than remote file\n"
			 "  -o, --output=DEST    store in remote file/directory DEST\n"
			 "  -p, --preserve       try to preserve file attributes\n"
			 "  -P, --parents        append source path to destination\n"
			 "  -q, --quiet          overrides --verbose\n"
			 "  -r, --recursive      upload directories recursively\n"
			 "  -R, --resume         resume broken transfer (restart at EOF)\n"
			 "  -s, --skip-existing  always skip existing files\n"
			 "  -t, --tagged         transfer (locally) tagged file(s)\n"
			 "      --type=TYPE      specify transfer type, 'ascii' or 'binary'\n"
			 "  -v, --verbose        explain what is being done\n"
			 "  -u, --unique         store in unique filename (if server supports STOU)\n"
			 "      --help           display this help\n"));
}

static int do_the_put(const char *src, const char *dest,
					  putmode_t how, unsigned opt)
{
	int r;
	transfer_mode_t type;

	if(test(opt, PUT_NOHUP))
		fprintf(stderr, "%s\n", src);

	type = ascii_transfer(src) ? tmAscii : gvDefaultType;
	if(test(opt, PUT_ASCII))
		type = tmAscii;
	else if(test(opt, PUT_BINARY))
		type = tmBinary;

#if 0 && (defined(HAVE_SETPROCTITLE) || defined(linux))
	if(gvUseEnvString && ftp_connected())
		setproctitle("%s, put %s", ftp->url->hostname, src);
#endif
	r = ftp_putfile(src, dest, how, type,
					test(opt, PUT_VERBOSE) ? transfer : 0);
#if 0 && (defined(HAVE_SETPROCTITLE) || defined(linux))
	if(gvUseEnvString && ftp_connected())
		setproctitle("%s", ftp->url->hostname);
#endif

	if(test(opt, PUT_NOHUP)) {
		if(r == 0)
			transfer_mail_msg(_("sent %s\n"), src);
		else
			transfer_mail_msg(_("failed to send %s: %s\n"), src, ftp_getreply(false));
	}
	
	return r;
}

static void putfile(const char *path, struct stat *sb,
					unsigned opt, const char *output)
{
	putmode_t how = putNormal;
	bool file_exists = false;
	char *dest, *dpath;
	int r;
	bool dir_created;
	char *dest_dir;

	if((put_glob_mask && fnmatch(put_glob_mask, base_name_ptr(path), FNM_EXTMATCH) == FNM_NOMATCH)
#ifdef HAVE_REGEX
	   || (put_rx_mask_set && regexec(&put_rx_mask, base_name_ptr(path), 0, 0, 0) == REG_NOMATCH)
#endif
		)
		return;

	if(!output)
		output = ".";

	if(test(opt, PUT_PARENTS)) {
		char *p = base_dir_xptr(path);
		asprintf(&dest, "%s/%s/%s", output, p, base_name_ptr(path));
		xfree(p);
	} else if(test(opt, PUT_OUTPUT_FILE))
		dest = xstrdup(output);
	else
		asprintf(&dest, "%s/%s", output, base_name_ptr(path));

	path_collapse(dest);

	/* make sure destination directory exists */
	dpath = base_dir_xptr(dest);
	dest_dir = ftp_path_absolute(dpath);
	r = ftp_mkpath(dest_dir);
	xfree(dest_dir);
	if(r == -1) {
		transfer_mail_msg(_("failed to create directory %s\n"), dest_dir);
		xfree(dpath);
		xfree(dest);
		return;
	}
	dir_created = (r == 1);

	if(!dir_created && !test(opt, PUT_UNIQUE) && !test(opt, PUT_FORCE)) {
		rfile *f;
		f = ftp_get_file(dest);
		file_exists = (f != 0);
		if(f && risdir(f)) {
			/* can't overwrite a directory */
			printf(_("%s: is a directory\n"), dest);
			xfree(dest);
			return;
		}
	}

	if(test(opt, PUT_APPEND)) {
		how = putAppend;
	} else if(file_exists) {
		if(test(opt, PUT_SKIP_EXISTING)) {
			printf(_("Remote file '%s' exists, skipping...\n"),
				   shortpath(dest, 42, ftp->homedir));
			xfree(dest);
			return;
		}
		else if(test(opt, PUT_NEWER)) {
			time_t ft = ftp_filetime(dest);
			if(ft != (time_t)-1 && ft >= sb->st_mtime) {
				printf(_("Remote file '%s' is newer than local, skipping...\n"),
					   shortpath(dest, 42, ftp->homedir));
				xfree(dest);
				return;
			}
		}
		else if(!test(opt, PUT_RESUME)) {
			if(!put_owbatch) {
				int a = ask(ASKYES|ASKNO|ASKUNIQUE|ASKCANCEL|ASKALL|ASKRESUME,
							ASKRESUME,
							_("File '%s' exists, overwrite?"),
							shortpath(dest, 42, ftp->homedir));
				if(a == ASKCANCEL) {
					put_quit = true;
					xfree(dest);
					return;
				}
				else if(a == ASKNO) {
					xfree(dest);
					return;
				}
				else if(a == ASKUNIQUE)
					opt |= PUT_UNIQUE; /* for this file only */
				else if(a == ASKALL)
					put_owbatch = true;
				else if(a == ASKRESUME)
					opt |= PUT_RESUME; /* for this file only */
				/* else a == ASKYES */
			}
		}
	}

	if(test(opt, PUT_RESUME))
		how = putResume;
	if(test(opt, PUT_UNIQUE))
		how = putUnique;

	r = do_the_put(path, dest, how, opt);
	xfree(dest);
	if(r != 0)
		return;

	if(test(opt, PUT_PRESERVE)) {
		if(ftp->has_site_chmod_command)
			ftp_chmod(ftp->ti.local_name, get_mode_string(sb->st_mode));
	}

	if(test(opt, PUT_DELETE_AFTER)) {
		bool dodel = false;

		if(!test(opt, PUT_FORCE) && !put_delbatch) {
			int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
						_("Delete local file '%s'?"),
						shortpath(path, 42, gvLocalHomeDir));
			if(a == ASKALL) {
				put_delbatch = true;
				dodel = true;
			}
			else if(a == ASKCANCEL)
				put_quit = true;
			else if(a != ASKNO)
				dodel = true;
		} else
			dodel = true;

		if(dodel) {
			if(unlink(path) == 0)
				printf(_("%s: deleted\n"),
					   shortpath(path, 42, gvLocalHomeDir));
			else
				printf(_("error deleting '%s': %s\n"),
					   shortpath(path, 42, gvLocalHomeDir),
					   strerror(errno));
		}
	}
}

static void putfiles(const list *gl, unsigned opt, const char *output)
{
	struct stat sb;
	char *path = 0;
	const char *file;
	listitem *li;

	for(li=gl->first; li && !put_quit; li=li->next) {

		if(!ftp_connected())
			return;

		if(gvSighupReceived) {
			if(!test(opt, PUT_RESUME))
				opt |= PUT_UNIQUE;
			opt |= PUT_FORCE;
		}

		path = (char *)li->data;
		file = base_name_ptr(path);

		if(strcmp(file, ".") == 0 || strcmp(file, "..") == 0)
			continue;

		if(test(opt, PUT_INTERACTIVE) && !put_batch) {
			int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
						_("Put '%s'?"),
						shortpath(path, 42, gvLocalHomeDir));
			if(a == ASKNO)
				continue;
			if(a == ASKCANCEL) {
				put_quit = true;
				break;
			}
			if(a == ASKALL)
				put_batch = true;
			/* else a==ASKYES */
		}

		if(stat(path, &sb) != 0) {
			perror(path);
			continue;
		}

		if(S_ISDIR(sb.st_mode)) {
			if(test(opt, PUT_RECURSIVE)) {
				char *recurs_output;
				char *recurs_mask;
				list *rgl;
				int r;

				if((put_dir_glob_mask
					&& fnmatch(put_dir_glob_mask,
							   base_name_ptr(path),
							   FNM_EXTMATCH) == FNM_NOMATCH)
#ifdef HAVE_REGEX
				   || (put_dir_rx_mask_set
					   && regexec(&put_dir_rx_mask,
								  base_name_ptr(path),
								  0, 0, 0) == REG_NOMATCH)
#endif
					)
					{
						/*printf("skipping %s\n", path);*/
					} else {
						if(!test(opt, PUT_PARENTS)) {
							asprintf(&recurs_output, "%s/%s",
									 output ? output : ".", file);
						} else
							recurs_output = xstrdup(output ? output : ".");

						asprintf(&recurs_mask, "%s/*", path);
						rgl = lglob_create();
						r = lglob_glob(rgl,
									   recurs_mask, lglob_exclude_dotdirs);
						xfree(recurs_mask);

						if(list_numitem(rgl) > 0)
							putfiles(rgl, opt, recurs_output);
						xfree(recurs_output);
					}
			} else
				fprintf(stderr, _("%s: omitting directory\n"),
					   shortpath(path, 42, gvLocalHomeDir));
			continue;
		}
		if(!S_ISREG(sb.st_mode)) {
			fprintf(stderr, _("%s: not a regular file\n"),
					shortpath(path, 42, gvLocalHomeDir));
			continue;
		}
		putfile(path, &sb, opt, output);

		if(gvInterrupted) {
			gvInterrupted = false;
			if(li->next && !put_quit && ftp_connected() && !gvSighupReceived)
			{
				int a = ask(ASKYES|ASKNO, ASKYES,
							_("Continue transfer?"));
				if(a == ASKNO) {
					put_quit = true;
					break;
				}
				/* else a == ASKYES */
				fprintf(stderr, _("Excellent!!!\n"));
			}
		}
	}
}

/* store a local file on remote server */
void cmd_put(int argc, char **argv)
{
	int c, opt=0;
	list *gl;
	char *put_output = 0;
	char *logfile = 0;
	pid_t pid;
#ifdef HAVE_REGEX
	int ret;
	char put_rx_errbuf[129];
#endif
	struct option longopts[] = {
		{"append", no_argument, 0, 'a'},
		{"delete-after", no_argument, 0, 'D'},
		{"dir-mask", required_argument, 0, '3'},
#ifdef HAVE_REGEX
		{"dir-rx-mask", required_argument, 0, '4'},
#endif
		{"force", no_argument, 0, 'f'},
		{"nohup", no_argument, 0, 'H'},
		{"interactive", no_argument, 0, 'i'},
		{"logfile", required_argument, 0, 'L'},
		{"mask", required_argument, 0, 'm'},
#ifdef HAVE_REGEX
		{"rx-mask", required_argument, 0, 'M'},
#endif
		{"newer", no_argument, 0, 'n'},
		{"output", required_argument, 0, 'o'},
		{"preserve", no_argument, 0, 'p'},
		{"parents", no_argument, 0, 'P'},
		{"quiet", no_argument, 0, 'q'},
		{"recursive", no_argument, 0, 'r'},
		{"resume", no_argument, 0, 'R'},
		{"skip-existing", no_argument, 0, 's'},
		{"tagged", no_argument, 0, 't'},
		{"type", required_argument, 0, '1'},
		{"verbose", no_argument, 0, 'v'},
		{"unique", no_argument, 0, 'u'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0},
	};

	if(put_glob_mask) {
		xfree(put_glob_mask);
		put_glob_mask = 0;
	}
	if(put_dir_glob_mask) {
		xfree(put_dir_glob_mask);
		put_dir_glob_mask = 0;
	}
#ifdef HAVE_REGEX
	if(put_rx_mask_set) {
		regfree(&put_rx_mask);
		put_rx_mask_set = 0;
	}
	if(put_dir_rx_mask_set) {
		regfree(&put_dir_rx_mask);
		put_dir_rx_mask_set = 0;
	}
#endif

	optind = 0; /* force getopt() to re-initialize */
	while((c = getopt_long(argc, argv,
						   "aDfHiL:no:pPqrRstvum:M:", longopts, 0)) != EOF)
	{
		switch(c) {
		case 'i':
			opt |= PUT_INTERACTIVE;
			break;
		case 'f':
			opt |= PUT_FORCE;
			break;
		case '3': /* --dir-mask=GLOB */
			xfree(put_dir_glob_mask);
			put_dir_glob_mask = xstrdup(optarg);
			unquote(put_dir_glob_mask);
			break;
#ifdef HAVE_REGEX
		case '4': /* --dir-rx-mask=REGEXP */
			if(put_dir_rx_mask_set) {
				regfree(&put_dir_rx_mask);
				put_dir_rx_mask_set = false;
			}
			unquote(optarg);
			ret = regcomp(&put_dir_rx_mask, optarg, REG_EXTENDED);
			if(ret != 0) {
				regerror(ret, &put_dir_rx_mask, put_rx_errbuf, 128);
				ftp_err(_("Regexp '%s' failed: %s\n"), optarg, put_rx_errbuf);
				return;
			} else
				put_dir_rx_mask_set = true;
			break;
#endif
		case 'o':
			put_output = tilde_expand_home(optarg, ftp->homedir);
			path_collapse(put_output);
			stripslash(put_output);
			break;
		case 'H':
			opt |= PUT_NOHUP;
			break;
		case 'L':
			xfree(logfile);
			logfile = xstrdup(optarg);
			unquote(logfile);
			break;
		case 'm': /* --mask */
			xfree(put_glob_mask);
			put_glob_mask = xstrdup(optarg);
			break;
#ifdef HAVE_REGEX
		case 'M': /* --rx-mask */
			if(put_rx_mask_set) {
				regfree(&put_rx_mask);
				put_rx_mask_set = false;
			}

			ret = regcomp(&put_rx_mask, optarg, REG_EXTENDED);
			if(ret != 0) {
				regerror(ret, &put_rx_mask, put_rx_errbuf, 128);
				ftp_err(_("Regexp '%s' failed: %s\n"), optind, put_rx_errbuf);
				return;
			} else
				put_rx_mask_set = true;
			break;
#endif
		  case 'n':
			opt |= PUT_NEWER;
			break;
		  case 'v':
			opt |= PUT_VERBOSE;
			break;
		  case 'q':
			opt &= ~PUT_VERBOSE;
			break;
		  case 'a':
			opt |= PUT_APPEND;
			break;
		  case 'D':
			opt |= PUT_DELETE_AFTER;
			break;
		  case 'u':
			opt |= PUT_UNIQUE;
			if(!ftp->has_stou_command) {
				fprintf(stderr, _("Remote doesn't support the STOU (store unique) command\n"));
				return;
			}
			break;
		  case 'r':
			opt |= PUT_RECURSIVE;
			break;
		  case 's':
			opt |= PUT_SKIP_EXISTING;
			break;
		  case 'R':
			opt |= PUT_RESUME;
			break;
		  case 't':
			opt |= PUT_TAGGED;
			break;
		  case '1':
			if(strncmp(optarg, "ascii", strlen(optarg)) == 0)
				opt |= PUT_ASCII;
			else if(strncmp(optarg, "binary", strlen(optarg)) == 0)
				opt |= PUT_BINARY;
			else {
				printf(_("Invalid option argument --type=%s\n"), optarg);
				return;
			}
			break;
		  case 'p':
			opt |= PUT_PRESERVE;
			break;
		  case 'P':
			opt |= PUT_PARENTS;
			break;
		  case 'h':
			print_put_syntax();;
			return;
		  case '?':
			return;
		}
	}
	if(optind>=argc && !test(opt, PUT_TAGGED)) {
/*		fprintf(stderr, _("missing argument, try 'put --help' for more information\n"));*/
		minargs(optind);
		return;
	}

	if(test(opt, PUT_APPEND) && test(opt, PUT_SKIP_EXISTING)) {
		printf("Can't use --append and --skip-existing simultaneously\n");
		return;
	}
	
	need_connected();
	need_loggedin();

	gl = lglob_create();
	while(optind < argc) {
		char *f;

		f = tilde_expand_home(argv[optind], gvLocalHomeDir);
		stripslash(f);
		lglob_glob(gl, f, lglob_exclude_dotdirs);
		optind++;
	}

	if(list_numitem(gl) == 0) {
		if(!test(opt, PUT_TAGGED)) {
			list_free(gl);
			return;
		} else if(list_numitem(gvLocalTagList) == 0) {
			printf(_("no tagged files\n"));
			list_free(gl);
			return;
		}
	}

	xfree(ftp->last_mkpath);
	ftp->last_mkpath = 0;

	put_quit = false;
	put_batch = put_owbatch = put_delbatch = test(opt, PUT_FORCE);
	if(test(opt, PUT_FORCE))
		opt &= ~PUT_INTERACTIVE;

	if(put_output && !test(opt, PUT_RECURSIVE) && list_numitem(gl) +
	   (test(opt, PUT_TAGGED) ? list_numitem(gvLocalTagList) : 0) == 1)
		{
			opt |= PUT_OUTPUT_FILE;
		}
	
	gvInTransfer = true;
	gvInterrupted = false;

	if(test(opt, PUT_NOHUP)) {
		int r = 0;
		pid = fork();

		if(pid == 0) {
			r = transfer_init_nohup(logfile);
			if(r != 0)
				exit(0);
		}

		if(r != 0)
			return;

		if(pid == 0) { /* child process */
			transfer_begin_nohup(argc, argv);

			if(!test(opt, PUT_FORCE) && !test(opt, PUT_RESUME))
				opt |= PUT_UNIQUE;
			opt |= PUT_FORCE;

			putfiles(gl, opt, put_output);
			list_free(gl);
			if(test(opt, PUT_TAGGED)) {
				putfiles(gvLocalTagList, opt, put_output);
				list_clear(gvLocalTagList);
			}
			xfree(put_output);

			transfer_end_nohup();
		}
		if(pid == -1) {
			perror("fork()");
			return;
		}
		/* parent process */
		sleep(1);
		printf("%d\n", pid);
		input_save_history();
		gvars_destroy();
		reset_xterm_title();
		exit(0);
	}
	
	putfiles(gl, opt, put_output);
	list_free(gl);
	if(test(opt, PUT_TAGGED)) {
		putfiles(gvLocalTagList, opt, put_output);
		list_clear(gvLocalTagList);
	}
	xfree(put_output);
	gvInTransfer = false;
}

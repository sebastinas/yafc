/* fxp.c -- transfer files between hosts
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
#include "input.h"
#include "transfer.h"
#include "gvars.h"
#include "commands.h"
#include "strq.h"
#include "shortpath.h"


#ifdef HAVE_REGEX_H
# include <regex.h>
#endif

#define FXP_INTERACTIVE 1
#define FXP_APPEND (1 << 1)
#define FXP_PRESERVE (1 << 2)
#define FXP_PARENTS (1 << 3)
#define FXP_RECURSIVE (1 << 4)
#define FXP_VERBOSE (1 << 5)
#define FXP_FORCE (1 << 6)
#define FXP_NEWER (1 << 7)
#define FXP_DELETE_AFTER (1 << 8)
#define FXP_UNIQUE (1 << 9)
#define FXP_RESUME (1 << 10)
#define FXP_TAGGED (1 << 11)
#define FXP_NOHUP (1 << 12)
#define FXP_SKIP_EXISTING (1 << 13)
#define FXP_BACKGROUND (1 << 14)
#define FXP_ASCII (1 << 15)
#define FXP_BINARY (1 << 16)

static Ftp *fxp_target = 0;
static bool fxp_batch = false;
static bool fxp_owbatch = false;
static bool fxp_delbatch = false;
static bool fxp_quit = false;

static char *fxp_glob_mask = 0;
static char *fxp_dir_glob_mask = 0;
#ifdef HAVE_REGEX
static regex_t fxp_rx_mask;
static bool fxp_rx_mask_set = false;
static regex_t fxp_dir_rx_mask;
static bool fxp_dir_rx_mask_set = false;
#endif

/* in put.c, FIXME: clean up! */
char* get_mode_string(mode_t m);

/* in commands.c */
listitem *ftplist_search(const char *str);

/* in cmd.c */
void reset_xterm_title(void);

static void print_fxp_syntax(void)
{
	printf(_("Transfers files between two remote servers.  Usage:\n"
			 "  fxp [options] files\n"
			 "Options:\n"
			 "  -a, --append         append if destination exists\n"
			 "  -D, --delete-after   delete source file after successful transfer\n"
			 "      --dir-mask=GLOB  enter only directories matching GLOB pattern\n"
			 "      --dir-rx-mask=REGEXP\n"
			 "                       enter only directories matching REGEXP pattern\n"
			 "  -f, --force          overwrite existing destinations, never prompt\n"
			 "  -H, --nohup          transfer files in background (nohup mode), quits yafc\n"
			 "  -i, --interactive    prompt before each transfer\n"
			 "  -L, --logfile=FILE   use FILE as logfile instead of ~/.yafc/nohup/nohup.<pid>\n"
			 "  -m, --mask=GLOB      get only files matching GLOB pattern\n"
			 "  -M, --rx-mask=REGEXP get only files matching REGEXP pattern\n"
			 "  -n, --newer          get file if destination is newer than source file\n"
			 "  -o, --output=DEST    store in destination directory DEST\n"
			 "  -p, --preserve       try to preserve file attributes\n"
			 "  -P, --parents        append source path to destination\n"
			 "  -q, --quiet          overrides --verbose\n"
			 "  -r, --recursive      get directories recursively\n"
			 "  -R, --resume         resume broken download (restart at eof)\n"
			 "  -s, --skip-existing  skip file if destination exists\n"
			 "  -t, --tagged         transfer tagged files\n"
			 "      --type=TYPE      specify transfer type, 'ascii' or 'binary'\n"
			 "  -u, --unique         store in unique filename (if target supports STOU)\n"
			 "  -v, --verbose        explain what is being done\n"
			 "      --help           display this help\n"));
}

static void fxp_preserve_attribs(const rfile *fi, char *dest)
{
	mode_t m = rfile_getmode(fi);
	if(m != (mode_t)-1) {
		/* ftp_use(fxp_target); */
		if(ftp->has_site_chmod_command)
			ftp_chmod(dest, get_mode_string(m));
	}
}

static int do_the_fxp(Ftp *srcftp, const char *src,
					  Ftp *destftp, const char *dest,
					  fxpmode_t how, unsigned opt)
{
	int r;
	transfer_mode_t type;

	if(test(opt, FXP_NOHUP))
		fprintf(stderr, "%s\n", src);

	type = ascii_transfer(src) ? tmAscii : gvDefaultType;
	if(test(opt, FXP_ASCII))
		type = tmAscii;
	else if(test(opt, FXP_BINARY))
		type = tmBinary;

#if defined(HAVE_SETPROCTITLE) || defined(linux)
	if(gvUseEnvString && ftp_connected())
		setproctitle("%s, fxp %s", srcftp->url->hostname, src);
#endif
	if(test(opt, FXP_VERBOSE)) {
		printf("%s\n", src);
	}
	r = ftp_fxpfile(srcftp, src, destftp, dest, how, type);
#if defined(HAVE_SETPROCTITLE) || defined(linux)
	if(gvUseEnvString && ftp_connected())
		setproctitle("%s", srcftp->url->hostname);
#endif

	if(test(opt, FXP_NOHUP)) {
		if(r == 0)
			transfer_mail_msg(_("sent %s\n"), src);
		else
			transfer_mail_msg(_("failed to send %s: %s\n"),
							  src, ftp_getreply(false));
	}
	
	return r;
}

static int fxpfile(const rfile *fi, unsigned int opt,
					const char *output, const char *destname)
{
	fxpmode_t how = fxpNormal;
	bool file_exists = false;
	char *dest, *dpath;
	int r;
	bool dir_created;
	char *dest_dir;
	Ftp *thisftp = ftp;

	if((fxp_glob_mask
		&& fnmatch(fxp_glob_mask, base_name_ptr(fi->path), 0) == FNM_NOMATCH)
#ifdef HAVE_REGEX
	   || (fxp_rx_mask_set
		   && regexec(&fxp_rx_mask,
					  base_name_ptr(fi->path), 0, 0, 0) == REG_NOMATCH)
#endif
		)
		return 0;

	if(!output)
		output = ".";

	if(test(opt, FXP_PARENTS)) {
		char *p = base_dir_xptr(fi->path);
		asprintf(&dest, "%s/%s/%s", output, p, base_name_ptr(fi->path));
		xfree(p);
	} else
		asprintf(&dest, "%s/%s", output, base_name_ptr(fi->path));

	path_collapse(dest);

	ftp_use(fxp_target);
	
	/* make sure destination directory exists */
	dpath = base_dir_xptr(dest);
	dest_dir = ftp_path_absolute(dpath);
	r = ftp_mkpath(dest_dir);
	xfree(dest_dir);
	if(r == -1) {
		transfer_mail_msg(_("failed to create directory %s\n"), dest_dir);
		xfree(dpath);
		xfree(dest);
		ftp_use(thisftp);
		return -1;
	}
	dir_created = (r == 1);

	if(!dir_created && !test(opt, FXP_UNIQUE) && !test(opt, FXP_FORCE)) {
		rfile *f;
		f = ftp_get_file(dest);
		file_exists = (f != 0);
		if(f && risdir(f)) {
			/* can't overwrite a directory */
			printf(_("%s: is a directory\n"), dest);
			xfree(dest);
			return 0;
		}
	}

	if(test(opt, FXP_APPEND)) {
		how = fxpAppend;
	} else if(file_exists) {
		if(test(opt, FXP_SKIP_EXISTING)) {
			printf(_("Remote file '%s' exists, skipping...\n"),
				   shortpath(dest, 42, ftp->homedir));
			xfree(dest);
			ftp_use(thisftp);
			return 0;
		}
		else if(test(opt, FXP_NEWER)) {
			time_t src_ft;
			time_t dst_ft;

			ftp_use(thisftp);
			src_ft = ftp_filetime(fi->path);
			ftp_use(fxp_target);
			
			dst_ft = ftp_filetime(dest);

			if(src_ft != (time_t)-1 && dst_ft != (time_t)-1 && dst_ft >= src_ft) {
				printf(_("Remote file '%s' is newer than local, skipping...\n"),
					   shortpath(dest, 42, ftp->homedir));
				xfree(dest);
				ftp_use(thisftp);
				return 0;
			}
		}
		else if(!test(opt, FXP_RESUME)) {
			if(!fxp_owbatch) {
				int a = ask(ASKYES|ASKNO|ASKUNIQUE|ASKCANCEL|ASKALL|ASKRESUME,
							ASKRESUME,
							_("File '%s' exists, overwrite?"),
							shortpath(dest, 42, ftp->homedir));
				if(a == ASKCANCEL) {
					fxp_quit = true;
					xfree(dest);
					ftp_use(thisftp);
					return 0;
				}
				else if(a == ASKNO) {
					xfree(dest);
					ftp_use(thisftp);
					return 0;
				}
				else if(a == ASKUNIQUE)
					opt |= FXP_UNIQUE; /* for this file only */
				else if(a == ASKALL)
					fxp_owbatch = true;
				else if(a == ASKRESUME)
					opt |= FXP_RESUME; /* for this file only */
				/* else a == ASKYES */
			}
		}
	}

	if(test(opt, FXP_RESUME))
		how = fxpResume;
	if(test(opt, FXP_UNIQUE))
		how = fxpUnique;

	r = do_the_fxp(thisftp, fi->path, fxp_target, dest, how, opt);
	xfree(dest);
	if(r != 0) {
		ftp_use(thisftp);
		return -1;
	}

	if(test(opt, FXP_PRESERVE))
		fxp_preserve_attribs(fi, dest);

	if(test(opt, FXP_DELETE_AFTER)) {
		bool dodel = false;

		ftp_use(thisftp);
		
		if(!test(opt, FXP_FORCE)
		   && !fxp_delbatch && !gvSighupReceived)
			{
				int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
							_("Delete remote file '%s'?"),
							shortpath(fi->path, 42, ftp->homedir));
				if(a == ASKALL) {
					fxp_delbatch = true;
					dodel = true;
				}
				else if(a == ASKCANCEL)
					fxp_quit = true;
				else if(a != ASKNO)
					dodel = true;
			} else
				dodel = true;

		if(dodel) {
			ftp_unlink(fi->path);
			if(ftp->code == ctComplete)
				fprintf(stderr, _("%s: deleted\n"),
						shortpath(fi->path, 42, ftp->homedir));
			else
				fprintf(stderr, _("error deleting '%s': %s\n"),
						shortpath(fi->path, 42, ftp->homedir),
						ftp_getreply(false));
		}
/*		ftp_use(fxp_target);*/
	}

	ftp_use(thisftp);
	return 0;
}

static void fxpfiles(list *gl, unsigned int opt, const char *output)
{
	listitem *li;
	rfile *fp, *lnfp;
	const char *opath, *ofile;
	char *link = 0;
	int r;

	li = gl->first;
	while(li && !fxp_quit) {
		fp = (rfile *)li->data;

		if(!ftp_connected())
			return;

		if(gvSighupReceived) {
			if(!test(opt, FXP_RESUME))
				opt |= FXP_UNIQUE;
			opt |= FXP_FORCE;
		}

		opath = fp->path;
		ofile = base_name_ptr(opath);

		if(strcmp(ofile, ".")==0 || strcmp(ofile, "..")==0) {
			transfer_nextfile(gl, &li, true);
			continue;
		}

		if(test(opt, FXP_INTERACTIVE) && !fxp_batch && !gvSighupReceived) {
			int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
						_("Get '%s'?"),
						shortpath(opath, 42, ftp->homedir));
			if(a == ASKNO) {
				transfer_nextfile(gl, &li, true);
				continue;
			}
			if(a == ASKCANCEL) {
				fxp_quit = true;
				break;
			}
			if(a == ASKALL)
				fxp_batch = true;
			/* else a==ASKYES */
		}

		r = 0;

		if(rislink(fp)) {
			link_to_link__duh:
			{
				char *xcurdir = base_dir_xptr(opath);
				link = path_absolute(fp->link, xcurdir, ftp->homedir);
				stripslash(link);
				xfree(xcurdir);
				ftp_trace("found link: '%s' -> '%s'\n", opath, link);
			}

			lnfp = ftp_get_file(link);
			if(lnfp == 0) {
				/* couldn't dereference the link, try to RETR it */
				ftp_trace("unable to dereference link\n");
				r = fxpfile(fp, opt, output, ofile);
				transfer_nextfile(gl, &li, r == 0);
				continue;
			}

			if(strncmp(opath, lnfp->path, strlen(lnfp->path)) == 0) {
				ftp_trace("opath == '%s', lnfp->path == '%s'\n", opath,
						  lnfp->path);
				fprintf(stderr, _("%s: circular link -- skipping\n"), 
						shortpath(lnfp->path, 42, ftp->homedir));
				transfer_nextfile(gl, &li, true);
				continue;
			}

			fp = lnfp;

			if(rislink(fp))
				/* found a link pointing to another link
				 */
				goto link_to_link__duh;
		}
		
		if(risdir(fp)) {
			if(test(opt, FXP_RECURSIVE)) {
				char *recurs_output;
				char *recurs_mask;
				list *rgl;

				if((fxp_dir_glob_mask
					&& fnmatch(fxp_dir_glob_mask,
							   base_name_ptr(fp->path),
							   FNM_EXTMATCH) == FNM_NOMATCH)
#ifdef HAVE_REGEX
				   || (fxp_dir_rx_mask_set
					   && regexec(&fxp_dir_rx_mask, base_name_ptr(fp->path),
								  0, 0, 0) == REG_NOMATCH)
#endif
					)
					{
						/*printf("skipping %s\n", fp->path);*/
					} else {
						if(!test(opt, FXP_PARENTS))
							asprintf(&recurs_output, "%s/%s",
									 output ? output : ".", ofile);
						else
							asprintf(&recurs_output, "%s",
									 output ? output : ".");
						rgl = rglob_create();
						asprintf(&recurs_mask, "%s/*", opath);
						rglob_glob(rgl, recurs_mask, true, true, 0);
						if(list_numitem(rgl) > 0)
							fxpfiles(rgl, opt, recurs_output);
						if(test(opt, FXP_PRESERVE))
							fxp_preserve_attribs(fp, recurs_output);
						rglob_destroy(rgl);
						xfree(recurs_output);
					}
			} else if(test(opt, FXP_VERBOSE)) {
				fprintf(stderr, _("%s: omitting directory\n"),
						shortpath(opath, 42, ftp->homedir));
			}
			transfer_nextfile(gl, &li, true);
			continue;
		}
		if(!risreg(fp)) {
			if(test(opt, FXP_VERBOSE))
				fprintf(stderr, _("%s: not a regular file\n"),
						shortpath(opath, 42, ftp->homedir));
			transfer_nextfile(gl, &li, true);
			continue;
		}
		r = fxpfile(fp, opt, output, ofile);

		transfer_nextfile(gl, &li, r == 0);

		if(gvInterrupted) {
			gvInterrupted = false;
			if(li && !fxp_quit && ftp_connected() && !gvSighupReceived)
			{
				int a = ask(ASKYES|ASKNO, ASKYES,
							_("Continue transfer?"));
				if(a == ASKNO) {
					fxp_quit = true;
					break;
				}
				/* else a == ASKYES */
				fprintf(stderr, _("Excellent!!!\n"));
			}
		}
	}
}

void cmd_fxp(int argc, char **argv)
{
	list *gl;
	listitem *fxp_tmp = 0;
	char *logfile = 0;
	char *fxp_output = 0;
#ifdef HAVE_REGEX
	int ret;
	char fxp_rx_errbuf[129];
#endif
	int c, opt = 0;
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
		{"target", required_argument, 0, 'T'},
		{"type", required_argument, 0, '1'},
		{"unique", no_argument, 0, 'u'},
		{"verbose", no_argument, 0, 'v'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	fxp_target = 0;

	if(fxp_glob_mask) {
		xfree(fxp_glob_mask);
		fxp_glob_mask = 0;
	}
	if(fxp_dir_glob_mask) {
		xfree(fxp_dir_glob_mask);
		fxp_dir_glob_mask = 0;
	}
#ifdef HAVE_REGEX
	if(fxp_rx_mask_set) {
		fxp_rx_mask_set = 0;
	}
	if(fxp_dir_rx_mask_set) {
		regfree(&fxp_dir_rx_mask);
		fxp_dir_rx_mask_set = 0;
	}
#endif

	optind = 0; /* force getopt() to re-initialize */
	while((c=getopt_long(argc, argv, "aDfHiL:M:no:pPqrRstT:uvh",
						 longopts, 0)) != EOF)
		{
			switch(c) {
			case 'a': /* --append */
				opt |= FXP_APPEND;
				break;
			case 'D': /* --delete-after */
				opt |= FXP_DELETE_AFTER;
				break;
			case 'f': /* --force */
				opt |= FXP_FORCE;
				break;
			case '3': /* --dir-mask=GLOB */
				xfree(fxp_dir_glob_mask);
				fxp_dir_glob_mask = xstrdup(optarg);
				unquote(fxp_dir_glob_mask);
				break;
#ifdef HAVE_REGEX
			case '4': /* --dir-rx-mask=REGEXP */
				if(fxp_dir_rx_mask_set) {
					regfree(&fxp_dir_rx_mask);
					fxp_dir_rx_mask_set = false;
				}
				unquote(optarg);
				ret = regcomp(&fxp_dir_rx_mask, optarg, REG_EXTENDED);
				if(ret != 0) {
					regerror(ret, &fxp_dir_rx_mask, fxp_rx_errbuf, 128);
					ftp_err(_("Regexp '%s' failed: %s\n"),
							optarg, fxp_rx_errbuf);
					return;
				} else
					fxp_dir_rx_mask_set = true;
				break;
#endif
			case 'H': /* --nohup */
				opt |= FXP_NOHUP;
				break;
			case 'i': /* --interactive */
				opt |= FXP_INTERACTIVE;
				break;
			case 'L': /* --logfile=FILE */
				xfree(logfile);
				logfile = xstrdup(optarg);
				unquote(logfile);
				break;
			case 'm': /* --mask=GLOB */
				xfree(fxp_glob_mask);
				fxp_glob_mask = xstrdup(optarg);
				unquote(fxp_glob_mask);
				break;
#ifdef HAVE_REGEX
			case 'M': /* --rx-mask=REGEXP */
				if(fxp_rx_mask_set) {
					regfree(&fxp_rx_mask);
					fxp_rx_mask_set = false;
				}

				unquote(optarg);
				ret = regcomp(&fxp_rx_mask, optarg, REG_EXTENDED);
				if(ret != 0) {
					regerror(ret, &fxp_rx_mask, fxp_rx_errbuf, 128);
					ftp_err(_("Regexp '%s' failed: %s\n"),
							optarg, fxp_rx_errbuf);
					return;
				} else
					fxp_rx_mask_set = true;
				break;
#endif
			case 'n': /* --newer */
				opt |= FXP_NEWER;
				break;
			case 'o': /* --output=DIRECTORY */
				if(fxp_target == 0) {
					printf(_("FxP target not set, use --target=NAME (as first option)\n"));
					return;
				}
				fxp_output = tilde_expand_home(optarg, fxp_target->homedir);
				stripslash(fxp_output);
				unquote(fxp_output);
				break;
			case 'p': /* --preserve */
				opt |= FXP_PRESERVE;
				break;
			case 'P': /* --parents */
				opt |= FXP_PARENTS;
				break;
			case 'q': /* --quiet */
				opt &= ~FXP_VERBOSE;
				break;
			case 'r': /* --recursive */
				opt |= FXP_RECURSIVE;
				break;
			case 'R': /* --resume */
				opt |= FXP_RESUME;
				break;
			case 't': /* --tagged */
				opt |= FXP_TAGGED;
				break;
			case '1': /* --type=[ascii|binary] */
				if(strncmp(optarg, "ascii", strlen(optarg)) == 0)
					opt |= FXP_ASCII;
				else if(strncmp(optarg, "binary", strlen(optarg)) == 0)
					opt |= FXP_BINARY;
				else {
					printf(_("Invalid option argument --type=%s\n"), optarg);
					return;
				}
				break;
			case 'T': /* --target=HOST */
				fxp_tmp = ftplist_search(optarg);
				if(!fxp_tmp)
					return;
				fxp_target = (Ftp *)fxp_tmp->data;
				break;
			case 'u': /* --unique */
				opt |= FXP_UNIQUE;
				break;
			case 'v': /* --verbose */
				opt |= FXP_VERBOSE;
				break;
			case 'h': /* --help */
				print_fxp_syntax();
				return;
			case '?':
			default:
				return;
			}
		}

	if(optind >= argc && !test(opt, FXP_TAGGED)) {
		minargs(optind);
		return;
	}
	
	need_connected();
	need_loggedin();

	if(fxp_target == 0) {
		ftp_err(_("No target specified, try '%s --help' for more information\n"), argv[0]);
		return;
	}
	
	gl = rglob_create();
	while(optind < argc) {
		stripslash(argv[optind]);
		if(rglob_glob(gl, argv[optind], true, true, 0) == -1)
			fprintf(stderr, _("%s: no matches found\n"), argv[optind]);
		optind++;
	}
	if(list_numitem(gl) == 0 && !test(opt, FXP_TAGGED)) {
		rglob_destroy(gl);
		return;
	}
	if(test(opt, FXP_TAGGED)
	   && (!ftp->taglist || list_numitem(ftp->taglist) == 0))
	{
		printf(_("no tagged files\n"));
		if(list_numitem(gl) == 0) {
			rglob_destroy(gl);
			return;
		}
	}

	fxp_quit = false;
	fxp_batch = fxp_owbatch = fxp_delbatch = test(opt, FXP_FORCE);
	if(test(opt, FXP_FORCE))
		opt &= ~FXP_INTERACTIVE;

	gvInTransfer = true;
	gvInterrupted = false;

	if(test(opt, FXP_NOHUP)) {
		int r = 0;
		pid_t pid = fork();

		if(pid == 0) {
			r = transfer_init_nohup(logfile);
			if(r != 0)
				exit(0);
		}

		if(r != 0)
			return;

		if(pid == 0) { /* child process */
			transfer_begin_nohup(argc, argv);

			if(!test(opt, FXP_FORCE) && !test(opt, FXP_RESUME))
				opt |= FXP_UNIQUE;
			opt |= FXP_FORCE;

			if(list_numitem(gl))
				fxpfiles(gl, opt, fxp_output);
			rglob_destroy(gl);

			if(ftp->taglist && test(opt, FXP_TAGGED))
				fxpfiles(ftp->taglist, opt, fxp_output);

			xfree(fxp_output);

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

	if(list_numitem(gl))
		fxpfiles(gl, opt, fxp_output);
	rglob_destroy(gl);

	if(ftp->taglist && test(opt, FXP_TAGGED))
		fxpfiles(ftp->taglist, opt, fxp_output);

	xfree(fxp_output);
	gvInTransfer = false;
}

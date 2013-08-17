/*
 * get.c -- get file(s) from remote
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
#include "get.h"
#include "ftp.h"
#include "gvars.h"
#include "strq.h"
#include "input.h"
#include "shortpath.h"
#include "transfer.h"
#include "commands.h"
#include "utils/modechange.h"
#include "utils.h"
#include "utils/bashline.h"

#ifdef HAVE_REGEX_H
# include <regex.h>
#endif

static bool get_quit = false;
static bool get_owbatch = false;
static bool get_delbatch = false;
static mode_change *cmod = 0;
static gid_t group_change = -1;
static bool get_skip_empty = false;

static char *get_glob_mask = 0;
static char *get_dir_glob_mask = 0;
#ifdef HAVE_REGEX
static regex_t get_rx_mask;
static bool get_rx_mask_set = false;
static regex_t get_dir_rx_mask;
static bool get_dir_rx_mask_set = false;
#endif

static void print_get_syntax(void)
{
  show_help(_("Receives files from remote."), "get [options] files",
    _("  -a, --append         append if destination exists\n"
      "  -c, --chmod=PERM     change mode of transferred files to PERM;\n"
      "                       PERM can be permissions in its octal representation\n"
      "                       or of the form [ugoa]*[+-=][rwxXstugo]+ separated by commas\n"
      "      --chgrp=GROUP    change group of transferred files to GROUP\n"
      "  -d, --no-dereference copy symbolic links as symbolic links\n"
      "  -D, --delete-after   delete remote file after successful transfer\n"
      "      --dir-mask=GLOB  enter only directories matching GLOB pattern\n"
      "      --dir-rx-mask=REGEXP\n"
      "                       enter only directories matching REGEXP pattern\n"
      "  -f, --force          overwrite existing destinations, never prompt\n"
      "  -e, --skip-empty     skip empty files\n"
      "  -H, --nohup          transfer files in background (nohup mode), quits yafc\n"
      "  -i, --interactive    prompt before each transfer\n"
      "  -L, --logfile=FILE   use FILE as logfile instead of ~/.yafc/nohup/nohup.<pid>\n"
      "  -m, --mask=GLOB      get only files matching GLOB pattern\n"
      "  -M, --rx-mask=REGEXP get only files matching REGEXP pattern\n"
      "  -n, --newer          get file if remote is newer than local file\n"
      "  -o, --output=DEST    store in local file/directory DEST\n"
      "  -p, --preserve       try to preserve file attributes and timestamps\n"
      "  -P, --parents        append source path to destination\n"
      "  -q, --quiet          overrides --verbose\n"
      "  -r, --recursive      get directories recursively\n"
      "  -R, --resume         resume broken download (restart at eof)\n"
      "  -s, --skip-existing  skip file if destination exists\n"
      "  -S, --stats[=NUM]    set stats transfer threshold; default is always\n"
      "  -t, --tagged         transfer tagged file(s)\n"
      "      --type=TYPE      specify transfer type, 'ascii' or 'binary'\n"
      "  -u, --unique         always store as unique local file\n"
      "  -v, --verbose        explain what is being done\n"));
}

static bool get_exclude_func(rfile *f)
{
    if(risdotdir(f))
        return true;
    if(get_skip_empty == true && !risdir(f) && f->size == 0)
       return true;
    return false;
}

/* just gets the file SRC and store in local file DEST
 * doesn't parse any LIST output
 * returns 0 on success, else -1
 */
static int do_the_get(const char *src, const char *dest,
                      putmode_t how, unsigned opt)
{
    char *fulldest;
    char *tmp;
    transfer_mode_t type;

    type = ascii_transfer(src) ? tmAscii : gvDefaultType;
    if(test(opt, GET_ASCII))
        type = tmAscii;
    else if(test(opt, GET_BINARY))
        type = tmBinary;

    tmp = getcwd(NULL, 0);
    if (tmp == (char *)NULL)
        return -1;

    fulldest = path_absolute(dest, tmp, 0);

#if 0 && (defined(HAVE_SETPROCTITLE) || defined(linux))
    if(gvUseEnvString && ftp_connected())
        setproctitle("%s, get %s", ftp->url->hostname, src);
#endif

    int r = ftp_getfile(src, dest, how, type,
                        test(opt, GET_VERBOSE)
                        && !gvSighupReceived
                        && !test(opt, GET_NOHUP) ? transfer : 0);

    if(r == 0 && (test(opt, GET_NOHUP) || gvSighupReceived)) {
        fprintf(stderr, "%s [%sb of ",
                src, human_size(ftp->ti.size));
        fprintf(stderr, "%sb]\n", human_size(ftp->ti.total_size));
    }
    if(test(opt, GET_NOHUP)) {
        if(r == 0)
            transfer_mail_msg(_("received %s\n"), src);
        else
            transfer_mail_msg(_("failed to receive %s: %s\n"),
                              src, ftp_getreply(false));
    }
    free(fulldest);
#if 0 && (defined(HAVE_SETPROCTITLE) || defined(linux))
    if(gvUseEnvString && ftp_connected())
        setproctitle("%s", ftp->url->hostname);
#endif

    free(tmp);

    return r;
}

static void get_preserve_attribs(const rfile *fi, char *dest)
{
    time_t t;
    mode_t m = rfile_getmode(fi);
    if(m != (mode_t)-1) {
        if(access(dest, F_OK) == 0 && chmod(dest, m) != 0)
            perror(dest);
    }
    if(!risdir(fi)) {
        t = ftp_filetime(fi->path);
        if(t != (time_t)-1) {
            struct utimbuf u;
            u.actime = t;
            u.modtime = t;
            if(utime(dest, &u) != 0)
                perror(dest);
        }
    }
}

/* returns:
 * 0   ok, remove file from list
 * -1  failure
 */
static int getfile(const rfile *fi, unsigned int opt,
             const char *output, const char *destname)
{
    struct stat sb;
    char* dest = NULL;
    getmode_t how = getNormal;
    bool mkunique = false;
    int r, ret = -1;

    if((get_glob_mask && fnmatch(get_glob_mask, base_name_ptr(fi->path),
                                 FNM_EXTMATCH) == FNM_NOMATCH)
#ifdef HAVE_REGEX
       || (get_rx_mask_set && regexec(&get_rx_mask, base_name_ptr(fi->path),
                                      0, 0, 0) == REG_NOMATCH)
#endif
        )
    {
        return 0;
    }

    if(!output)
        output = ".";

    if(test(opt, GET_PARENTS)) {
        char *apath = base_dir_xptr(fi->path);
        if (asprintf(&dest, "%s%s/%s", output, apath, destname) == -1)
        {
          free(apath);
          fprintf(stderr, _("Failed to allocate memory.\n"));
          return -1;
        }
        free(apath);
    } else {
        /* check if -o option is given, if GET_OUTPUT_FILE is set, we only
         * transfer one file and output is set to the filename. However, if
         * the destination already exists and is a directory, we assume
         * that the user meant a directory */

        int dest_is_file = test(opt, GET_OUTPUT_FILE);

        if(stat(output, &sb) == 0) {
            if(S_ISDIR(sb.st_mode)) {
                dest_is_file = false;
            }
        }

        if(dest_is_file)
            dest = xstrdup(output);
        else
            if (asprintf(&dest, "%s/%s", output, destname) == -1)
            {
              fprintf(stderr, _("Failed to allocate memory.\n"));
              return -1;
            }
    }

    /* make sure destination directory exists */
    {
        char *destdir = base_dir_xptr(dest);
        if(destdir) {
            bool r = make_path(destdir);
            if(!r) {
                if (errno == EEXIST)
                  ftp_err("`%s' exists but is not a directory\n", destdir);
                else
                  ftp_err("%s: %s\n", destdir, strerror(errno));
                transfer_mail_msg(_("Couldn't create directory: %s\n"),
                                  destdir);
                free(destdir);
                return -1;
            }
            /* change permission and group, if requested */
            if(test(opt, GET_CHMOD)) {
                if(stat(destdir, &sb) == 0) {
                    mode_t m = sb.st_mode;
                    m = mode_adjust(m, cmod);
                    if(chmod(destdir, m) != 0)
                        perror(destdir);
                }           }
            if(test(opt, GET_CHGRP)) {
                if(chown(destdir, -1, group_change) != 0)
                    perror(dest);
            }
            free(destdir);
        }
    }

    /* check if destination file exists */
    if(stat(dest, &sb) == 0) {
        if(test(opt, GET_SKIP_EXISTING)) {
            if(test(opt, GET_VERBOSE)) {
							char* sp = shortpath(dest, 42, gvLocalHomeDir);
              fprintf(stderr, _("Local file '%s' exists, skipping...\n"),
                      sp);
							stats_file(STATS_SKIP, 0);
							free(sp);
						}
            return 0;
        }
        if(test(opt, GET_UNIQUE))
            mkunique = true;
        else if(test(opt, GET_APPEND))
            how = getAppend;
        else if(test(opt, GET_NEWER)) {
            struct tm *fan = gmtime(&sb.st_mtime);
            time_t ft = ftp_filetime(fi->path);
            sb.st_mtime = gmt_mktime(fan);

            ftp_trace("get -n: remote file: %s", ctime(&ft));
            ftp_trace("get -n: local file: %s\n", ctime(&sb.st_mtime));

            if(sb.st_mtime >= ft && ft != (time_t)-1) {
                if(test(opt, GET_VERBOSE)) {
									char* sp = shortpath(dest, 30, gvLocalHomeDir);
                    ftp_err(_(
                        "Local file '%s' is newer than remote, skipping...\n"),
                            sp);
									stats_file(STATS_SKIP, 0);
									free(sp);
								}
                return 0;
            }
        } else if(!test(opt, GET_RESUME)) {
            if(!get_owbatch && !gvSighupReceived) {
                struct tm *fan = gmtime(&sb.st_mtime);
                time_t ft = ftp_filetime(fi->path);
                int a;
                char *e;

                sb.st_mtime = gmt_mktime(fan);
                e = xstrdup(ctime(&sb.st_mtime));
								char* sp = shortpath(dest, 42, gvLocalHomeDir);
                a = ask(ASKYES|ASKNO|ASKUNIQUE|ASKCANCEL|ASKALL|ASKRESUME,
                        ASKRESUME,
                        _("Local file '%s' exists\nLocal: %lld bytes, %sRemote: %lld bytes, %sOverwrite?"),
                        sp,
                        (unsigned long long) sb.st_size, e ? e : "unknown date\n",
                        ftp_filesize(fi->path), ctime(&ft));
								free(sp);
                free(e);
                if(a == ASKCANCEL) {
                    get_quit = true;
                    return 0;
                }
                else if(a == ASKNO)
                    return 0;
                else if(a == ASKUNIQUE)
                    mkunique = true;
                else if(a == ASKALL) {
                    get_owbatch = true;
                }
                else if(a == ASKRESUME)
                    opt |= GET_RESUME; /* for this file only */
                /* else a == ASKYES */
            }
        }
        if(test(opt, GET_RESUME))
            how = getResume;
    }

    if(mkunique) {
        char* newdest = make_unique_filename(dest);
        free(dest);
        dest = newdest;
    }

    /* the file doesn't exist or we choosed to overwrite it, or changed dest */

    if(rislink(fi) && test(opt, GET_NO_DEREFERENCE)) {
        /* remove any existing destination */
        unlink(dest);
        ftp_err(_("symlinking '%s' to '%s'\n"), dest, fi->link);
        if(symlink(fi->link, dest) != 0)
            perror(dest);
        ret = 0;
    }
    else {
        r = do_the_get(fi->path, dest, how, opt);

        if(r == 0) {
			stats_file(STATS_SUCCESS, ftp->ti.total_size);
            ret = 0;
            if(test(opt, GET_PRESERVE))
                get_preserve_attribs(fi, dest);
            if(test(opt, GET_CHMOD)) {
                mode_t m = rfile_getmode(fi);
                m = mode_adjust(m, cmod);
                if(chmod(dest, m) != 0)
                    perror(dest);
            }
            if(test(opt, GET_CHGRP)) {
                if(chown(dest, -1, group_change) != 0)
                    perror(dest);
            }
            if(test(opt, GET_DELETE_AFTER)) {
                bool dodel = false;
								char* sp = shortpath(fi->path, 42, ftp->homedir);
                if(!test(opt, GET_FORCE)
                   && !get_delbatch && !gvSighupReceived)
                {
                    int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
                                _("Delete remote file '%s'?"), sp);
                    if(a == ASKALL) {
                        get_delbatch = true;
                        dodel = true;
                    }
                    else if(a == ASKCANCEL)
                        get_quit = true;
                    else if(a != ASKNO)
                        dodel = true;
                } else
                    dodel = true;

                if(dodel) {
                    ftp_unlink(fi->path);
                    if(ftp->code == ctComplete)
                        fprintf(stderr, _("%s: deleted\n"), sp);
                    else
                        fprintf(stderr, _("error deleting '%s': %s\n"),
															  sp, ftp_getreply(false));
                }
								free(sp);
            }
        } else {
			stats_file(STATS_FAIL, 0);
			ret = -1;
		}
    }

    free(dest);
    return ret;
}

static bool get_batch = false;

int get_sort_func(const void *a, const void *b)
{
   const rfile *ra = (const rfile *)a;
   const rfile *rb = (const rfile *)b;
   bool tfa = transfer_first(ra->path);
   bool tfb = transfer_first(rb->path);

   if(tfa)
      return tfb ? 0 : -1;
   return tfb ? 1 : 0;
}

static void getfiles(list *gl, unsigned int opt, const char *output)
{
    listitem *li;
    rfile *fp, *lnfp;
    const char *opath, *ofile;
    char *link = 0;

    list_sort(gl, get_sort_func, false);

    li = gl->first;
    while(li && !get_quit) {
        fp = (rfile *)li->data;

        if(!ftp_connected())
            return;

        if(gvSighupReceived) {
            if(!test(opt, GET_RESUME))
                opt |= GET_UNIQUE;
            opt |= GET_FORCE;
        }

        opath = fp->path;
        ofile = base_name_ptr(opath);

        if(strcmp(ofile, ".")==0 || strcmp(ofile, "..")==0) {
            transfer_nextfile(gl, &li, true);
            continue;
        }

        if(test(opt, GET_INTERACTIVE) && !get_batch && !gvSighupReceived) {
						char* sp = shortpath(opath, 42, ftp->homedir);
            int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
                        _("Get '%s'?"), sp);
						free(sp);
            if(a == ASKNO) {
                transfer_nextfile(gl, &li, true);
                continue;
            }
            if(a == ASKCANCEL) {
                get_quit = true;
                break;
            }
            if(a == ASKALL)
                get_batch = true;
            /* else a==ASKYES */
        }


        if(rislink(fp)) {
            link_to_link__duh:
            if(test(opt, GET_NO_DEREFERENCE)) {
                /* link the file, don't copy */
                const int r = getfile(fp, opt, output, ofile);
                transfer_nextfile(gl, &li, r == 0);
                continue;
            }

            {
                char *xcurdir = base_dir_xptr(opath);
                link = path_absolute(fp->link, xcurdir, ftp->homedir);
                stripslash(link);
                free(xcurdir);
                ftp_trace("found link: '%s' -> '%s'\n", opath, link);
            }

            lnfp = ftp_get_file(link);
            if(lnfp == 0) {
                /* couldn't dereference the link, try to RETR it */
                ftp_trace("unable to dereference link\n");
                const int r = getfile(fp, opt, output, ofile);
                transfer_nextfile(gl, &li, r == 0);
                continue;
            }

            if(strncmp(opath, lnfp->path, strlen(lnfp->path)) == 0) {
                ftp_trace("opath == '%s', lnfp->path == '%s'\n", opath,
                          lnfp->path);
								char* sp = shortpath(lnfp->path, 42, ftp->homedir);
                fprintf(stderr, _("%s: circular link -- skipping\n"), sp);
								free(sp);
                transfer_nextfile(gl, &li, true);
                continue;
            }

            fp = lnfp;

            if(rislink(fp))
                /* found a link pointing to another link
                 */
                /* forgive me father, for I have goto'ed */
                goto link_to_link__duh;
        }

        if(risdir(fp)) {
            if(test(opt, GET_RECURSIVE)) {
                char *recurs_output;
                char *recurs_mask;
                list *rgl;


                if((get_dir_glob_mask && fnmatch(get_dir_glob_mask,
                                                 base_name_ptr(fp->path),
                                                 FNM_EXTMATCH) == FNM_NOMATCH)
#ifdef HAVE_REGEX
                   || (get_dir_rx_mask_set && regexec(&get_dir_rx_mask,
                                                      base_name_ptr(fp->path),
                                                      0, 0, 0) == REG_NOMATCH)
#endif
                    )
                    {
                    } else {
                        char *q_recurs_mask;

                        bool success = true;
                        if(!test(opt, GET_PARENTS))
                            success = asprintf(&recurs_output, "%s/%s",
                                     output ? output : ".", ofile) != -1;
                        else
                            success = asprintf(&recurs_output, "%s",
                                     output ? output : ".") != -1;
                        if (!success)
                        {
                          fprintf(stderr, _("Failed to allocate memory.\n"));
                          transfer_nextfile(gl, &li, true);
			                    continue;
                        }
                        if (asprintf(&recurs_mask, "%s/*", opath) == -1)
                        {
                          free(recurs_output);
                          fprintf(stderr, _("Failed to allocate memory.\n"));
                          transfer_nextfile(gl, &li, true);
                          continue;
                        }
                        rgl = rglob_create();
                        q_recurs_mask = backslash_quote(recurs_mask);
                        rglob_glob(rgl, q_recurs_mask, true, true, get_exclude_func);
                        free(q_recurs_mask);
                        if(list_numitem(rgl) > 0)
                            getfiles(rgl, opt, recurs_output);
                        if(test(opt, GET_PRESERVE))
                            get_preserve_attribs(fp, recurs_output);
                        rglob_destroy(rgl);
                        free(recurs_output);
                    }
            } else if(test(opt, GET_VERBOSE)) {
							char* sp = shortpath(opath, 42, ftp->homedir);
							fprintf(stderr, _("%s: omitting directory\n"), sp);
							free(sp);
            }
            transfer_nextfile(gl, &li, true);
            continue;
        }
        if(!risreg(fp)) {
            if(test(opt, GET_VERBOSE)) {
							char* sp = shortpath(opath, 42, ftp->homedir);
                fprintf(stderr, _("%s: not a regular file\n"), sp);
								free(sp);
						}
            transfer_nextfile(gl, &li, true);
            continue;
        }
        const int r = getfile(fp, opt, output, ofile);

        transfer_nextfile(gl, &li, r == 0);

        if(gvInterrupted) {
            gvInterrupted = false;
            if(li && !get_quit && ftp_connected() && !gvSighupReceived)
            {
                int a = ask(ASKYES|ASKNO, ASKYES,
                            _("Continue transfer?"));
                if(a == ASKNO) {
                    get_quit = true;
                    break;
                }
                /* else a == ASKYES */
                fprintf(stderr, _("Excellent!!!\n"));
            }
        }
    }
}

void cmd_get(int argc, char **argv)
{
    list *gl;
    int opt=GET_VERBOSE, c;
    char *logfile = 0;
    pid_t pid;
    struct group *grp;
    char *get_output = 0;
    int stat_thresh = gvStatsThreshold;
#ifdef HAVE_REGEX
    int ret;
    char get_rx_errbuf[129];
#endif
    struct option longopts[] = {
        {"append", no_argument, 0, 'a'},
        {"chmod", required_argument, 0, 'c'},
        {"chgrp", required_argument, 0, '2'},
        {"no-dereference", no_argument, 0, 'd'},
        {"delete-after", no_argument, 0, 'D'},
        {"dir-mask", required_argument, 0, '3'},
#ifdef HAVE_REGEX
        {"dir-rx-mask", required_argument, 0, '4'},
#endif
        {"interactive", no_argument, 0, 'i'},
        {"skip-empty", no_argument, 0, 'e'},
        {"force", no_argument, 0, 'f'},
        {"logfile", required_argument, 0, 'L'},
        {"mask", required_argument, 0, 'm'},
#ifdef HAVE_REGEX
        {"rx-mask", required_argument, 0, 'M'},
#endif
        {"newer", no_argument, 0, 'n'},
        {"nohup", no_argument, 0, 'H'},
        {"verbose", no_argument, 0, 'v'},
        {"preserve", no_argument, 0, 'p'},
        {"parents", no_argument, 0, 'P'},
        {"quiet", no_argument, 0, 'q'},
        {"recursive", no_argument, 0, 'r'},
        {"resume", no_argument, 0, 'R'},
        {"skip-existing", no_argument, 0, 's'},
        {"stats", optional_argument, 0, 'S'},
        {"tagged", no_argument, 0, 't'},
        {"type", required_argument, 0, '1'},
        {"unique", no_argument, 0, 'u'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
    };

    if(cmod) {
        mode_free(cmod);
        cmod = 0;
    }

    if(get_glob_mask) {
        free(get_glob_mask);
        get_glob_mask = 0;
    }
    if(get_dir_glob_mask) {
        free(get_dir_glob_mask);
        get_dir_glob_mask = 0;
    }
#ifdef HAVE_REGEX
    if(get_rx_mask_set) {
        regfree(&get_rx_mask);
        get_rx_mask_set = 0;
    }
    if(get_dir_rx_mask_set) {
        regfree(&get_dir_rx_mask);
        get_dir_rx_mask_set = 0;
    }
#endif

    get_skip_empty = false;

    optind = 0; /* force getopt() to re-initialize */
    while((c=getopt_long(argc, argv, "abHc:dDeio:fL:tnpPvqrRsuT:m:M:",
                         longopts, 0)) != EOF)
    {
        switch(c) {
          case 'a':
            opt |= GET_APPEND;
            break;
          case 'c':
            cmod = mode_compile(optarg, MODE_MASK_ALL);
            if(cmod == NULL) {
                fprintf(stderr, _("Invalid mode for --chmod: %s\n"), optarg);
                return;
            }
            opt |= GET_CHMOD;
            break;
        case '2': /* --chgrp */
            grp = getgrnam(optarg);
            if(grp == 0) {
                fprintf(stderr, _("%s is not a valid group name\n"), optarg);
                return;
            }
            {
                int i;
                for(i=0; grp->gr_mem && grp->gr_mem[i]; i++) {
                    if(strcmp(gvUsername, grp->gr_mem[i]) == 0)
                        break;
                }
                if(!grp->gr_mem[i]) {
                    fprintf(stderr,
                            _("you are not a member of group %s\n"), optarg);
                    return;
                }
            }
            group_change = grp->gr_gid;
            opt |= GET_CHGRP;
            break;
          case 'D':
            opt |= GET_DELETE_AFTER;
            break;
          case 'd':
            opt |= GET_NO_DEREFERENCE;
            break;
           case 'e':
              opt |= GET_SKIP_EMPTY;
              get_skip_empty = true;
              break;
        case '3': /* --dir-mask=GLOB */
            free(get_dir_glob_mask);
            get_dir_glob_mask = xstrdup(optarg);
            unquote(get_dir_glob_mask);
            break;
#ifdef HAVE_REGEX
        case '4': /* --dir-rx-mask=REGEXP */
            if(get_dir_rx_mask_set) {
                regfree(&get_dir_rx_mask);
                get_dir_rx_mask_set = false;
            }
            unquote(optarg);
            ret = regcomp(&get_dir_rx_mask, optarg, REG_EXTENDED);
            if(ret != 0) {
                regerror(ret, &get_dir_rx_mask, get_rx_errbuf, 128);
                ftp_err(_("Regexp '%s' failed: %s\n"), optarg, get_rx_errbuf);
                return;
            } else
                get_dir_rx_mask_set = true;
            break;
#endif
          case 'i':
            opt |= GET_INTERACTIVE;
            break;
          case 'f':
            opt |= GET_FORCE;
            break;
        case 'm': /* --mask */
            free(get_glob_mask);
            get_glob_mask = xstrdup(optarg);
            unquote(get_glob_mask);
            break;
#ifdef HAVE_REGEX
        case 'M': /* --rx-mask */
            if(get_rx_mask_set) {
                regfree(&get_rx_mask);
                get_rx_mask_set = false;
            }
            unquote(optarg);
            ret = regcomp(&get_rx_mask, optarg, REG_EXTENDED);
            if(ret != 0) {
                regerror(ret, &get_rx_mask, get_rx_errbuf, 128);
                ftp_err(_("Regexp '%s' failed: %s\n"), optarg, get_rx_errbuf);
                return;
            } else
                get_rx_mask_set = true;
            break;
#endif
          case 'o':
            get_output = tilde_expand_home(optarg, gvLocalHomeDir);
            /*stripslash(get_output);*/
            unquote(get_output);
            break;
          case 'v':
            opt |= GET_VERBOSE;
            break;
          case 'p':
            opt |= GET_PRESERVE;
            break;
          case 'P':
            opt |= GET_PARENTS;
            break;
          case 'H':
            opt |= GET_NOHUP;
            break;
          case 'q':
            opt &= ~GET_VERBOSE;
            break;
          case 'r':
            opt |= GET_RECURSIVE;
            break;
          case 's':
            opt |= GET_SKIP_EXISTING;
            break;
          case 'S':
            stat_thresh = optarg ? atoi(optarg) : 0;
            break;
          case 'R':
            opt |= GET_RESUME;
            break;
          case '1':
            if(strncmp(optarg, "ascii", strlen(optarg)) == 0)
                opt |= GET_ASCII;
            else if(strncmp(optarg, "binary", strlen(optarg)) == 0)
                opt |= GET_BINARY;
            else {
                printf(_("Invalid option argument --type=%s\n"), optarg);
                return;
            }
            break;
          case 'u':
            opt |= GET_UNIQUE;
            break;
          case 'L':
              free(logfile);
              logfile = xstrdup(optarg);
              unquote(logfile);
              break;
          case 't':
            opt |= GET_TAGGED;
            break;
          case 'n':
            opt |= GET_NEWER;
            break;
          case 'h':
            print_get_syntax();
            return;
          case '?':
            return;
        }
    }
    if(optind>=argc && !test(opt, GET_TAGGED)) {
        minargs(optind);
        return;
    }

    need_connected();
    need_loggedin();

    gl = rglob_create();
    while(optind < argc) {
        stripslash(argv[optind]);
        if(rglob_glob(gl, argv[optind], true, true, get_exclude_func) == -1)
            fprintf(stderr, _("%s: no matches found\n"), argv[optind]);
        optind++;
    }
    if(list_numitem(gl) == 0 && !test(opt, GET_TAGGED)) {
        rglob_destroy(gl);
        return;
    }
    if(test(opt, GET_TAGGED)
       && (!ftp->taglist || list_numitem(ftp->taglist)==0))
    {
        printf(_("no tagged files\n"));
        if(list_numitem(gl) == 0) {
            rglob_destroy(gl);
            return;
        }
    }

    get_quit = false;
    get_batch = get_owbatch = get_delbatch = test(opt, GET_FORCE);
    if(test(opt, GET_FORCE))
        opt &= ~GET_INTERACTIVE;

    if(get_output && !test(opt, GET_RECURSIVE) && list_numitem(gl) +
       (test(opt, GET_TAGGED) ? list_numitem(ftp->taglist) : 0) == 1)
        {
            /* if the argument to --output ends with a slash, we assume the
             * user wants the destination to be a directory
             */
            char *e = strrchr(get_output, 0);
            if(e && e[-1] != '/')
                opt |= GET_OUTPUT_FILE;
        }

    stats_reset(gvStatsTransfer);

    gvInTransfer = true;
    gvInterrupted = false;

    if(test(opt, GET_NOHUP)) {
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

            if(!test(opt, GET_FORCE) && !test(opt, GET_RESUME))
                opt |= GET_UNIQUE;
            opt |= GET_FORCE;

            if(list_numitem(gl))
                getfiles(gl, opt, get_output);
            rglob_destroy(gl);
            if(ftp->taglist && test(opt, GET_TAGGED))
                getfiles(ftp->taglist, opt, get_output);
            free(get_output);

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
        getfiles(gl, opt, get_output);
    rglob_destroy(gl);
    if(ftp->taglist && test(opt, GET_TAGGED))
        getfiles(ftp->taglist, opt, get_output);
    free(get_output);
    mode_free(cmod);
    cmod = 0;
    gvInTransfer = false;

    stats_display(gvStatsTransfer, stat_thresh);
}

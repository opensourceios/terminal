/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static char sccsid[] = "@(#)chflags.c	8.5 (Berkeley) 4/1/94";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/bin/chflags/chflags.c,v 1.26.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $");

#include <sys/types.h>
#include <sys/stat.h>

// #include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ios_error.h"


static void usage(void);

int
chflags_main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *p;
	u_long clear, newflags, set;
	long val;
	int Hflag, Lflag, Rflag, fflag, hflag, vflag;
	int ch, fts_options, oct, rval;
	char *flags, *ep;
	int (*change_flags)(const char *, u_int);

	Hflag = Lflag = Rflag = fflag = hflag = vflag = 0;
    optind = 1; opterr = 1; optreset = 1;
    
	while ((ch = getopt(argc, argv, "HLPRfhv")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'v':
			vflag++;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	if (Rflag) {
		fts_options = FTS_PHYSICAL;
        if (hflag) {
            fprintf(thread_stderr, "chflags: the -R and -h options may not be specified together\n");
            pthread_exit(NULL);
            //errx(1, "the -R and -h options "
            //        "may not be specified together");
        }
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else
		fts_options = hflag ? FTS_PHYSICAL : FTS_LOGICAL;

	if (hflag)
		change_flags = lchflags;
	else
		change_flags = chflags;

	flags = *argv;
	if (*flags >= '0' && *flags <= '7') {
		errno = 0;
		val = strtol(flags, &ep, 8);
		if (val < 0)
			errno = ERANGE;
        if (errno) {
            // err(1, "invalid flags: %s", flags);
            fprintf(thread_stderr, "chflags: %s: invalid flags: %s\n", flags, strerror(errno));
            pthread_exit(NULL);
        }
        if (*ep) {
            // errx(1, "invalid flags: %s", flags);
            fprintf(thread_stderr, "chflags: invalid flags: %s\n", flags);
            pthread_exit(NULL);
        }
		set = val;
                oct = 1;
	} else {
        if (strtofflags(&flags, &set, &clear)) {
                        // errx(1, "invalid flag: %s", flags);
            fprintf(thread_stderr, "chflags: invalid flag: %s\n", flags);
            pthread_exit(NULL); 
        }
		clear = ~clear;
		oct = 0;
	}

    if ((ftsp = fts_open(++argv, fts_options , 0)) == NULL) {
		// err(1, NULL);
        fprintf(thread_stderr, "chflags: %s\n", strerror(errno));
        pthread_exit(NULL);
    }

	for (rval = 0; (p = fts_read(ftsp)) != NULL;) {
		switch (p->fts_info) {
		case FTS_D:	/* Change it at FTS_DP if we're recursive. */
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chflag, continue. */
			// warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
            fprintf(thread_stderr, "chflags: %s: %s\n", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			// warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
            fprintf(thread_stderr, "chflags: %s: %s\n", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		case FTS_SL:			/* Ignore. */
		case FTS_SLNONE:
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything and ones that we found
			 * doing a physical walk.
			 */
			if (!hflag)
				continue;
			/* FALLTHROUGH */
		default:
			break;
		}
		if (oct)
			newflags = set;
		else
			newflags = (p->fts_statp->st_flags | set) & clear;
		if (newflags == p->fts_statp->st_flags)
			continue;
		if ((*change_flags)(p->fts_accpath, (u_int)newflags) && !fflag) {
			// warn("%s", p->fts_path);
            fprintf(thread_stderr,"chflags: %s: %s\n", p->fts_path, strerror(errno));
			rval = 1;
		} else if (vflag) {
			(void)fprintf(thread_stdout, "%s", p->fts_path);
			if (vflag > 1)
				(void)fprintf(thread_stdout, ": 0%lo -> 0%lo",
				    (u_long)p->fts_statp->st_flags,
				    newflags);
			(void)fprintf(thread_stdout, "\n");
		}
	}
    if (errno) {
		// err(1, "fts_read");
        fprintf(thread_stderr, "chflags: fts_read: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
	exit(rval);
}

static void
usage(void)
{
	(void)fprintf(thread_stderr,
	    "usage: chflags [-fhv] [-R [-H | -L | -P]] flags file ...\n");
	exit(1);
}

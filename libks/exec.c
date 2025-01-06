/*
 * Copyright (c) 2024 Anton Lindqvist <anton@basename.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "libks/exec.h"

#include <sys/wait.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "libks/fs.h"

int
KS_exec_diff(const char *path, const char *src, size_t srclen, const char *dst,
    size_t dstlen)
{
	char dstpath[PATH_MAX], srcpath[PATH_MAX], label[PATH_MAX];
	pid_t pid;
	int dstfd = -1;
	int srcfd = -1;
	int rv = -1;
	int n;

	n = snprintf(label, sizeof(label), "%s.orig", path);
	if (n < 0 || (size_t)n >= sizeof(label)) {
		errno = ENAMETOOLONG;
		goto out;
	}

	srcfd = KS_fs_tmpfd(src, srclen, srcpath, sizeof(srcpath));
	if (srcfd == -1)
		goto out;
	dstfd = KS_fs_tmpfd(dst, dstlen, dstpath, sizeof(dstpath));
	if (dstfd == -1)
		goto out;

	pid = fork();
	if (pid == -1)
		goto out;
	if (pid == 0) {
		execlp("diff", "diff", "-u",
		    "-L", label, "-L", path,
		    srcpath, dstpath, NULL);
		_exit(1);
	}

	if (waitpid(pid, NULL, 0) == -1)
		goto out;

	rv = 1;

out:
	if (srcfd != -1)
		close(srcfd);
	if (dstfd != -1)
		close(dstfd);
	return rv;
}

/*
 * Copyright (c) 2023 Anton Lindqvist <anton@basename.se>
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

#include "libks/fs.h"

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>	/* mkstemp(3) on Linux */
#include <string.h>
#include <unistd.h>

static int
construct_hidden_path(const char *path, char *buf, size_t bufsiz)
{
	char prefix[PATH_MAX];
	const char *p, *suffix;
	int n;

	p = strrchr(path, '/');
	if (p != NULL) {
		size_t len;

		len = (size_t)(&p[1] - path);
		n = snprintf(prefix, sizeof(prefix), "%.*s", (int)len, path);
		if (n < 0 || (size_t)n >= sizeof(prefix)) {
			errno = ENAMETOOLONG;
			return -1;
		}
		suffix = &p[1];
	} else {
		prefix[0] = '\0';
		suffix = path;
	}

	n = snprintf(buf, bufsiz, "%s.%s.XXXXXX", prefix, suffix);
	if (n < 0 || (size_t)n >= bufsiz) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

static int
inherit_ownership_and_permissions(int srcfd, int dstfd)
{
	struct stat dstsb, srcsb;

	if (fstat(srcfd, &srcsb) == -1)
		return -1;
	if (fstat(dstfd, &dstsb) == -1)
		return -1;

	if (srcsb.st_mode != dstsb.st_mode &&
	    fchmod(dstfd, srcsb.st_mode) == -1)
		return -1;
	if ((srcsb.st_uid != dstsb.st_uid || srcsb.st_gid != dstsb.st_gid) &&
	    fchown(dstfd, srcsb.st_uid, srcsb.st_gid) == -1)
		return -1;

	return 0;
}

int
KS_fs_replace(const char *path, const char *buf, size_t buflen)
{
	char tmppath[PATH_MAX] = {0};
	mode_t old_umask;
	int tmpfd = -1;
	int error = -1;
	int srcfd;

	srcfd = open(path, O_RDONLY | O_CLOEXEC);
	if (srcfd == -1)
		goto out;

	if (construct_hidden_path(path, tmppath, sizeof(tmppath)) == -1)
		goto out;

	old_umask = umask(0022);
	tmpfd = mkstemp(tmppath);
	umask(old_umask);
	if (tmpfd == -1)
		goto out;
	while (buflen > 0) {
		ssize_t nw;

		nw = write(tmpfd, buf, buflen);
		if (nw == -1)
			goto out;
		buf += nw;
		buflen -= (size_t)nw;
	}

	if (inherit_ownership_and_permissions(srcfd, tmpfd) == -1)
		goto out;

	if (rename(tmppath, path) == -1)
		goto out;

	error = 0;

out:
	if (tmppath[0] != '\0')
		(void)unlink(tmppath);
	if (srcfd != -1)
		close(srcfd);
	if (tmpfd != -1)
		close(tmpfd);
	return error;
}

int
KS_fs_tmpfd(const char *buf, size_t buflen, char *path, size_t pathsiz)
{
	char tmppath[PATH_MAX];
	size_t siz = sizeof(tmppath);
	mode_t old_umask;
	int fd = -1;
	int n;

	n = snprintf(tmppath, siz, "/tmp/libks.XXXXXXXX");
	if (n < 0 || (size_t)n >= siz) {
		errno = ENAMETOOLONG;
		return -1;
	}
	old_umask = umask(0022);
	fd = mkstemp(tmppath);
	umask(old_umask);
	if (fd == -1)
		return -1;
	if (unlink(tmppath) == -1)
		goto err;

	while (buflen > 0) {
		ssize_t nw;

		nw = write(fd, buf, buflen);
		if (nw == -1)
			goto err;
		buf += nw;
		buflen -= (size_t)nw;
	}
	if (lseek(fd, 0, SEEK_SET) == -1)
		goto err;

	siz = pathsiz;
	n = snprintf(path, siz, "/dev/fd/%d", fd);
	if (n < 0 || (size_t)n >= siz) {
		errno = ENAMETOOLONG;
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

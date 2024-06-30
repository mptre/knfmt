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

#include "libks/tmp.h"

#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>	/* mkstemp(3) on Linux */
#include <unistd.h>

int
KS_tmpfd(const char *buf, size_t buflen, char *path, size_t pathsiz)
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

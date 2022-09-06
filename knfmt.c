#include "config.h"

#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"
#include "diff.h"
#include "error.h"
#include "file.h"
#include "lexer.h"
#include "options.h"
#include "parser.h"
#include "vector.h"

#define _PATH_DIFF	"/usr/bin/diff"

static __dead void	usage(void);

static int	filelist(int, char **, struct files *, const struct options *);
static int	fileformat(struct file *, const struct options *);
static int	filediff(const struct buffer *, const struct buffer *,
    const char *);
static int	filewrite(const struct buffer *, const struct buffer *,
    const char *);
static int	fileattr(const char *, const char *);

static int	tmpfd(const char *, size_t, char *, size_t);

int
main(int argc, char *argv[])
{
	struct files files;
	struct options op;
	size_t i;
	int error = 0;
	int ch;

	if (pledge("stdio rpath wpath cpath fattr chown proc exec", NULL) == -1)
		err(1, "pledge");

	options_init(&op);

	while ((ch = getopt(argc, argv, "Ddisv")) != -1) {
		switch (ch) {
		case 'D':
			op.op_flags |= OPTIONS_FLAG_DIFFPARSE;
			break;
		case 'd':
			op.op_flags |= OPTIONS_FLAG_DIFF;
			break;
		case 'i':
			op.op_flags |= OPTIONS_FLAG_INPLACE;
			break;
		case 's':
			op.op_flags |= OPTIONS_FLAG_SIMPLE;
			break;
		case 'v':
			op.op_verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((op.op_flags & OPTIONS_FLAG_DIFFPARSE) && argc > 0)
		usage();

	if (op.op_flags & OPTIONS_FLAG_DIFF) {
		if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
			err(1, "pledge");
	} else if (op.op_flags & OPTIONS_FLAG_INPLACE) {
		if (pledge("stdio rpath wpath cpath fattr chown", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
	}

	diff_init();
	lexer_init();
	if (VECTOR_INIT(files.fs_vc) == NULL) {
		error = 1;
		goto out;
	}

	if (filelist(argc, argv, &files, &op)) {
		error = 1;
		goto out;
	}
	for (i = 0; i < VECTOR_LENGTH(files.fs_vc); i++) {
		struct file *fe = &files.fs_vc[i];

		if (fileformat(fe, &op)) {
			error = 1;
			error_flush(fe->fe_error, 1);
		}
	}

out:
	files_free(&files);
	lexer_shutdown();
	diff_shutdown();

	return error;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: knfmt [-Ddis] [file ...]\n");
	exit(1);
}

static int
filelist(int argc, char **argv, struct files *files,
    const struct options *op)
{
	if (op->op_flags & OPTIONS_FLAG_DIFFPARSE)
		return diff_parse(files, op);

	if (argc == 0) {
		files_alloc(files, "/dev/stdin", op);
	} else {
		int i;

		for (i = 0; i < argc; i++)
			files_alloc(files, argv[i], op);
	}
	return 0;
}

static int
fileformat(struct file *fe, const struct options *op)
{
	struct buffer *dst = NULL;
	struct buffer *src;
	struct lexer *lx = NULL;
	struct parser *pr = NULL;
	int error = 0;

	src = buffer_read(fe->fe_path);
	if (src == NULL) {
		error = 1;
		goto out;
	}
	lx = lexer_alloc(fe->fe_path, src, fe->fe_error, fe->fe_diff, op);
	if (lx == NULL) {
		error = 1;
		goto out;
	}
	pr = parser_alloc(fe->fe_path, lx, fe->fe_error, op);
	if (pr == NULL) {
		error = 1;
		goto out;
	}
	dst = parser_exec(pr, src->bf_len);
	if (dst == NULL) {
		error = 1;
		goto out;
	}

	if (op->op_flags & OPTIONS_FLAG_DIFF)
		error = filediff(src, dst, fe->fe_path);
	else if (op->op_flags & OPTIONS_FLAG_INPLACE)
		error = filewrite(src, dst, fe->fe_path);
	else
		printf("%.*s", (int)dst->bf_len, dst->bf_ptr);

out:
	buffer_free(dst);
	parser_free(pr);
	lexer_free(lx);
	buffer_free(src);
	return error;
}

static int
filediff(const struct buffer *src, const struct buffer *dst, const char *path)
{
	char dstpath[PATH_MAX], srcpath[PATH_MAX];
	pid_t pid;
	int dstfd = -1;
	int srcfd = -1;

	if (buffer_cmp(src, dst) == 0)
		return 0;

	srcfd = tmpfd(src->bf_ptr, src->bf_len, srcpath, sizeof(srcpath));
	if (srcfd == -1)
		goto out;
	dstfd = tmpfd(dst->bf_ptr, dst->bf_len, dstpath, sizeof(dstpath));
	if (dstfd == -1)
		goto out;

	pid = fork();
	if (pid == -1) {
		warn("fork");
		goto out;
	}
	if (pid == 0) {
		char label[PATH_MAX];
		ssize_t siz = sizeof(label);
		int n;

		n = snprintf(label, siz, "%s.orig", path);
		if (n < 0 || n >= siz)
			errc(1, ENAMETOOLONG, "%s: label", __func__);

		execl(_PATH_DIFF, _PATH_DIFF, "-u", "-L", label, "-L", path,
		    srcpath, dstpath, NULL);
		_exit(1);
	}

	if (waitpid(pid, NULL, 0) == -1) {
		warn("waitpid");
		goto out;
	}

out:
	if (srcfd != -1)
		close(srcfd);
	if (dstfd != -1)
		close(dstfd);
	return 1;
}

static int
filewrite(const struct buffer *src, const struct buffer *dst, const char *path)
{
	char tmppath[PATH_MAX];
	const char *buf;
	ssize_t siz = sizeof(tmppath);
	size_t len;
	int fd, n;

	if (buffer_cmp(src, dst) == 0)
		return 0;

	n = snprintf(tmppath, siz, "%s.XXXXXXXX", path);
	if (n < 0 || n >= siz) {
		warnc(ENAMETOOLONG, "%s", __func__);
		return 1;
	}
	fd = mkstemp(tmppath);
	if (fd == -1) {
		warn("mkstemp: %s", tmppath);
		return 1;
	}

	buf = dst->bf_ptr;
	len = dst->bf_len;
	while (len > 0) {
		ssize_t nw;

		nw = write(fd, buf, len);
		if (nw == -1) {
			warn("write: %s", tmppath);
			goto err;
		}
		buf += nw;
		len -= nw;
	}
	close(fd);
	fd = -1;

	if (fileattr(tmppath, path))
		goto err;

	/*
	 * Atomically replace the file using rename(2), matches what
	 * clang-format does.
	 */
	if (rename(tmppath, path) == -1) {
		warn("rename: %s", tmppath);
		goto err;
	}

	return 0;

err:
	if (fd != -1)
		close(fd);
	(void)unlink(tmppath);
	return 1;
}

static int
fileattr(const char *srcpath, const char *dstpath)
{
	struct stat dstst, srcst;

	if (stat(srcpath, &srcst) == -1) {
		warn("stat: %s", srcpath);
		return 1;
	}
	if (stat(dstpath, &dstst) == -1) {
		warn("stat: %s", dstpath);
		return 1;
	}

	if (srcst.st_mode != dstst.st_mode &&
	    chmod(srcpath, dstst.st_mode) == -1) {
		warn("chmod: %s", srcpath);
		return 1;
	}
	if ((srcst.st_uid != dstst.st_uid || srcst.st_gid != dstst.st_gid) &&
	    chown(srcpath, dstst.st_uid, dstst.st_gid) == -1) {
		warn("chown: %s", srcpath);
		return 1;
	}

	return 0;
}

/*
 * Get a read/write file descriptor by creating a temporary file and write out
 * the given buffer. The file is immediately removed in the hopes of returning
 * the last reference to the file.
 */
static int
tmpfd(const char *buf, size_t buflen, char *path, size_t pathsiz)
{
	char tmppath[PATH_MAX];
	ssize_t siz = sizeof(tmppath);
	int fd = -1;
	int n;

	n = snprintf(tmppath, siz, "/tmp/knfmt.XXXXXXXX");
	if (n < 0 || n >= siz) {
		warnc(ENAMETOOLONG, "%s: tmp", __func__);
		return -1;
	}
	fd = mkstemp(tmppath);
	if (fd == -1) {
		warn("mkstemp: %s", tmppath);
		return -1;
	}
	if (unlink(tmppath) == -1) {
		warn("unlink: %s", tmppath);
		goto err;
	}

	while (buflen > 0) {
		ssize_t nw;

		nw = write(fd, buf, buflen);
		if (nw == -1) {
			warn("write");
			goto err;
		}
		buf += nw;
		buflen -= nw;
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		warn("lseek");
		goto err;
	}

	siz = pathsiz;
	n = snprintf(path, siz, "/dev/fd/%d", fd);
	if (n < 0 || n >= siz) {
		warnc(ENAMETOOLONG, "%s: path", __func__);
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

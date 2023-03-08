#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"
#include "clang.h"
#include "diff.h"
#include "error.h"
#include "file.h"
#include "lexer.h"
#include "options.h"
#include "parser.h"
#include "style.h"
#include "token.h"
#include "vector.h"

static void	usage(void) __attribute__((__noreturn__));

static int	filelist(int, char **, struct files *, const struct options *);
static int	fileformat(struct file *, const struct style *,
    const struct options *);
static int	filediff(const struct buffer *, const struct buffer *,
    const struct file *);
static int	filewrite(const struct buffer *, const struct buffer *,
    const struct file *);
static int	fileprint(const struct buffer *);
static int	fileattr(const char *, int, const char *, int);

static int	tmpfd(const char *, size_t, char *, size_t);

int
main(int argc, char *argv[])
{
	struct files files;
	struct options op;
	struct style *st = NULL;
	const char *clang_format = NULL;
	size_t i;
	int error = 0;
	int ch;

	if (pledge("stdio rpath wpath cpath fattr chown proc exec", NULL) == -1)
		err(1, "pledge");

	options_init(&op);

	while ((ch = getopt(argc, argv, "c:Ddisv:")) != -1) {
		switch (ch) {
		case 'c':
			clang_format = optarg;
			break;
		case 'D':
			op.op_flags |= OPTIONS_DIFFPARSE;
			break;
		case 'd':
			op.op_flags |= OPTIONS_DIFF;
			break;
		case 'i':
			op.op_flags |= OPTIONS_INPLACE;
			break;
		case 's':
			op.op_flags |= OPTIONS_SIMPLE;
			break;
		case 'v':
			if (options_trace_parse(&op, optarg))
				return 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((op.op_flags & OPTIONS_DIFFPARSE) && argc > 0)
		usage();

	if (op.op_flags & OPTIONS_DIFF) {
		if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
			err(1, "pledge");
	} else if (op.op_flags & OPTIONS_INPLACE) {
		if (pledge("stdio rpath wpath cpath fattr chown", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
	}

	if (op.op_flags & OPTIONS_DIFFPARSE)
		diff_init();
	clang_init();
	style_init();
	if (VECTOR_INIT(files.fs_vc) == NULL) {
		error = 1;
		goto out;
	}
	st = style_parse(clang_format, &op);
	if (st == NULL) {
		error = 1;
		goto out;
	}

	if (filelist(argc, argv, &files, &op)) {
		error = 1;
		goto out;
	}
	for (i = 0; i < VECTOR_LENGTH(files.fs_vc); i++) {
		struct file *fe = &files.fs_vc[i];

		if (fileformat(fe, st, &op)) {
			error = 1;
			error_flush(fe->fe_error, 1);
		}
	}

out:
	files_free(&files);
	style_free(st);
	style_teardown();
	clang_shutdown();
	if (op.op_flags & OPTIONS_DIFFPARSE)
		diff_shutdown();

	return error;
}

static void
usage(void)
{
	fprintf(stderr, "usage: knfmt [-Ddis] [file ...]\n");
	exit(1);
}

static int
filelist(int argc, char **argv, struct files *files,
    const struct options *op)
{
	if (op->op_flags & OPTIONS_DIFFPARSE)
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
fileformat(struct file *fe, const struct style *st, const struct options *op)
{
	struct buffer *dst = NULL;
	struct buffer *src;
	struct clang *cl = NULL;
	struct lexer *lx = NULL;
	struct parser *pr = NULL;
	int error = 0;

	src = file_read(fe);
	if (src == NULL) {
		error = 1;
		goto out;
	}
	cl = clang_alloc(op, st);
	lx = lexer_alloc(&(const struct lexer_arg){
	    .path	= fe->fe_path,
	    .bf		= src,
	    .er		= fe->fe_error,
	    .diff	= fe->fe_diff,
	    .op		= op,
	    .callbacks	= {
		.read		= clang_read,
		.serialize	= token_serialize,
		.arg		= cl,
	    },
	});
	if (lx == NULL) {
		error = 1;
		goto out;
	}
	pr = parser_alloc(fe->fe_path, lx, fe->fe_error, st, op);
	if (pr == NULL) {
		error = 1;
		goto out;
	}
	dst = parser_exec(pr, buffer_get_len(src));
	if (dst == NULL) {
		error = 1;
		goto out;
	}

	if (op->op_flags & OPTIONS_DIFF)
		error = filediff(src, dst, fe);
	else if (op->op_flags & OPTIONS_INPLACE)
		error = filewrite(src, dst, fe);
	else
		error = fileprint(dst);

out:
	buffer_free(dst);
	parser_free(pr);
	lexer_free(lx);
	clang_free(cl);
	buffer_free(src);
	return error;
}

static int
filediff(const struct buffer *src, const struct buffer *dst,
    const struct file *fe)
{
	char dstpath[PATH_MAX], srcpath[PATH_MAX];
	pid_t pid;
	int dstfd = -1;
	int srcfd = -1;

	if (buffer_cmp(src, dst) == 0)
		return 0;

	srcfd = tmpfd(buffer_get_ptr(src), buffer_get_len(src),
	    srcpath, sizeof(srcpath));
	if (srcfd == -1)
		goto out;
	dstfd = tmpfd(buffer_get_ptr(dst), buffer_get_len(dst),
	    dstpath, sizeof(dstpath));
	if (dstfd == -1)
		goto out;

	pid = fork();
	if (pid == -1) {
		warn("fork");
		goto out;
	}
	if (pid == 0) {
		char label[PATH_MAX];
		const char *diff = "/usr/bin/diff";
		size_t siz = sizeof(label);
		int n;

		n = snprintf(label, siz, "%s.orig", fe->fe_path);
		if (n < 0 || (size_t)n >= siz)
			errc(1, ENAMETOOLONG, "%s: label", __func__);

		execl(diff, diff, "-u", "-L", label, "-L", fe->fe_path,
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
filewrite(const struct buffer *src, const struct buffer *dst,
    const struct file *fe)
{
	char tmppath[PATH_MAX];
	const char *buf;
	size_t siz = sizeof(tmppath);
	size_t buflen;
	mode_t old_umask;
	int fd, n;

	if (buffer_cmp(src, dst) == 0)
		return 0;

	n = snprintf(tmppath, siz, "%s.XXXXXXXX", fe->fe_path);
	if (n < 0 || (size_t)n >= siz) {
		warnc(ENAMETOOLONG, "%s", __func__);
		return 1;
	}
	old_umask = umask(0022);
	fd = mkstemp(tmppath);
	umask(old_umask);
	if (fd == -1) {
		warn("mkstemp: %s", tmppath);
		return 1;
	}

	buf = buffer_get_ptr(dst);
	buflen = buffer_get_len(dst);
	while (buflen > 0) {
		ssize_t nw;

		nw = write(fd, buf, buflen);
		if (nw == -1) {
			warn("write: %s", tmppath);
			goto err;
		}
		buf += nw;
		buflen -= (size_t)nw;
	}

	if (fileattr(tmppath, fd, fe->fe_path, fe->fe_fd))
		goto err;

	/*
	 * Atomically replace the file using rename(2), matches what
	 * clang-format does.
	 */
	if (rename(tmppath, fe->fe_path) == -1) {
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
fileprint(const struct buffer *dst)
{
	const char *buf = buffer_get_ptr(dst);
	size_t buflen = buffer_get_len(dst);

	while (buflen > 0) {
		ssize_t nw;

		nw = write(1, buf, buflen);
		if (nw == -1) {
			warn("write: /dev/stdout");
			return 1;
		}
		buflen -= (size_t)nw;
		buf += nw;
	}
	return 0;
}

static int
fileattr(const char *srcpath, int srcfd, const char *dstpath, int dstfd)
{
	struct stat dstsb, srcsb;

	if (fstat(srcfd, &srcsb) == -1) {
		warn("stat: %s", srcpath);
		return 1;
	}
	if (fstat(dstfd, &dstsb) == -1) {
		warn("stat: %s", dstpath);
		return 1;
	}

	if (srcsb.st_mode != dstsb.st_mode &&
	    fchmod(srcfd, dstsb.st_mode) == -1) {
		warn("chmod: %s", srcpath);
		return 1;
	}
	if ((srcsb.st_uid != dstsb.st_uid || srcsb.st_gid != dstsb.st_gid) &&
	    fchown(srcfd, dstsb.st_uid, dstsb.st_gid) == -1) {
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
	size_t siz = sizeof(tmppath);
	mode_t old_umask;
	int fd = -1;
	int n;

	n = snprintf(tmppath, siz, "/tmp/knfmt.XXXXXXXX");
	if (n < 0 || (size_t)n >= siz) {
		warnc(ENAMETOOLONG, "%s: tmp", __func__);
		return -1;
	}
	old_umask = umask(0022);
	fd = mkstemp(tmppath);
	umask(old_umask);
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
		buflen -= (size_t)nw;
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		warn("lseek");
		goto err;
	}

	siz = pathsiz;
	n = snprintf(path, siz, "/dev/fd/%d", fd);
	if (n < 0 || (size_t)n >= siz) {
		warnc(ENAMETOOLONG, "%s: path", __func__);
		goto err;
	}

	return fd;

err:
	close(fd);
	return -1;
}

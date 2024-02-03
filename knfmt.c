#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <limits.h>	/* PATH_MAX */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/tmp.h"
#include "libks/vector.h"

#include "clang.h"
#include "diff.h"
#include "expr.h"
#include "file.h"
#include "fs.h"
#include "lexer.h"
#include "options.h"
#include "parser.h"
#include "simple.h"
#include "style.h"
#include "token.h"

struct main_context {
	const struct options	*options;
	struct style		*style;
	struct simple		*simple;
	struct clang		*clang;
	struct buffer		*src;
	struct buffer		*dst;
};

static void	usage(void) __attribute__((__noreturn__));

static int	filelist(int, char **, struct files *, const struct options *);
static int	fileformat(struct main_context *, struct file *);
static int	filediff(const struct buffer *, const struct buffer *,
    const struct file *);
static int	filewrite(const struct buffer *, const struct buffer *,
    const struct file *);
static int	fileprint(const struct buffer *);
static int	fileattr(const char *, int, const char *, int);

int
main(int argc, char *argv[])
{
	struct files files;
	struct options op;
	struct arena *eternal, *scratch;
	struct buffer *src = NULL;
	struct buffer *dst = NULL;
	struct clang *cl = NULL;
	struct simple *si = NULL;
	struct style *st = NULL;
	const char *clang_format = NULL;
	size_t i;
	int error = 0;
	int ch;

	if (pledge("stdio rpath wpath cpath fattr chown proc exec", NULL) == -1)
		err(1, "pledge");

	options_init(&op);

	while ((ch = getopt(argc, argv, "c:Ddist:")) != -1) {
		switch (ch) {
		case 'c':
			clang_format = optarg;
			break;
		case 'D':
			op.diffparse = 1;
			break;
		case 'd':
			op.diff = 1;
			break;
		case 'i':
			op.inplace = 1;
			break;
		case 's':
			op.simple = 1;
			break;
		case 't':
			if (options_trace_parse(&op, optarg))
				return 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((op.diffparse && argc > 0) ||
	    (!op.diffparse && op.inplace && argc == 0))
		usage();

	if (op.diff) {
		if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
			err(1, "pledge");
	} else if (op.inplace) {
		if (pledge("stdio rpath wpath cpath fattr chown", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
	}

	if (op.diffparse)
		diff_init();
	clang_init();
	expr_init();
	style_init();
	eternal = arena_alloc(ARENA_FATAL);
	if (eternal == NULL)
		err(1, NULL);
	arena_scope(eternal, eternal_scope);
	scratch = arena_alloc(ARENA_FATAL);
	if (scratch == NULL)
		err(1, NULL);
	if (VECTOR_INIT(files.fs_vc))
		err(1, NULL);
	st = style_parse(clang_format, &eternal_scope, scratch, &op);
	if (st == NULL) {
		error = 1;
		goto out;
	}
	si = simple_alloc(&op);
	cl = clang_alloc(st, si, scratch, &op);
	src = buffer_alloc(1 << 12);
	if (src == NULL) {
		error = 1;
		goto out;
	}
	dst = buffer_alloc(1 << 12);
	if (dst == NULL) {
		error = 1;
		goto out;
	}

	if (filelist(argc, argv, &files, &op)) {
		error = 1;
		goto out;
	}

	struct main_context c = {
		.options	= &op,
		.style		= st,
		.simple		= si,
		.clang		= cl,
		.src		= src,
		.dst		= dst,
	};
	for (i = 0; i < VECTOR_LENGTH(files.fs_vc); i++) {
		struct file *fe = &files.fs_vc[i];

		if (fileformat(&c, fe))
			error = 1;
		file_close(fe);
	}

out:
	files_free(&files);
	buffer_free(dst);
	buffer_free(src);
	clang_free(cl);
	simple_free(si);
	style_free(st);
	arena_free(scratch);
	arena_free(eternal);
	style_shutdown();
	expr_shutdown();
	clang_shutdown();
	if (op.diffparse)
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
	if (op->diffparse)
		return diff_parse(files, op);

	if (argc == 0) {
		files_alloc(files, "/dev/stdin");
	} else {
		int i;

		for (i = 0; i < argc; i++)
			files_alloc(files, argv[i]);
	}
	return 0;
}

static int
fileformat(struct main_context *c, struct file *fe)
{
	struct lexer *lx = NULL;
	struct parser *pr = NULL;
	int error = 0;

	buffer_reset(c->src);
	if (file_read(fe, c->src)) {
		error = 1;
		goto out;
	}
	lx = lexer_alloc(&(const struct lexer_arg){
	    .path		= fe->fe_path,
	    .bf			= c->src,
	    .diff		= fe->fe_diff,
	    .op			= c->options,
	    .error_flush	= trace(c->options, 'l') > 0,
	    .callbacks		= {
		.read		= clang_read,
		.alloc		= clang_token_alloc,
		.serialize	= token_serialize,
		.arg		= c->clang,
	    },
	});
	if (lx == NULL) {
		error = 1;
		goto out;
	}
	pr = parser_alloc(lx, c->style, c->simple, c->options);
	if (pr == NULL) {
		error = 1;
		goto out;
	}
	buffer_reset(c->dst);
	if (parser_exec(pr, fe->fe_diff, c->dst)) {
		error = 1;
		goto out;
	}

	if (c->options->diff)
		error = filediff(c->src, c->dst, fe);
	else if (c->options->inplace)
		error = filewrite(c->src, c->dst, fe);
	else
		error = fileprint(c->dst);

out:
	if (lx != NULL && error)
		lexer_error_flush(lx);
	parser_free(pr);
	lexer_free(lx);
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

	srcfd = KS_tmpfd(buffer_get_ptr(src), buffer_get_len(src),
	    srcpath, sizeof(srcpath));
	if (srcfd == -1)
		goto out;
	dstfd = KS_tmpfd(buffer_get_ptr(dst), buffer_get_len(dst),
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
	const char *buf;
	char *tmppath;
	size_t buflen;
	mode_t old_umask;
	int fd;

	if (buffer_cmp(src, dst) == 0)
		return 0;

	tmppath = tmptemplate(fe->fe_path);
	old_umask = umask(0022);
	fd = mkstemp(tmppath);
	umask(old_umask);
	if (fd == -1) {
		warn("mkstemp: %s", tmppath);
		goto err;
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

	close(fd);
	free(tmppath);
	return 0;

err:
	if (fd != -1) {
		close(fd);
		(void)unlink(tmppath);
	}
	free(tmppath);
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

#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define _PATH_DIFF	"/usr/bin/diff"

static __dead void	usage(void);

static int	filelist(int, char **, struct file_list *,
    const struct config *);
static int	fileformat(const struct file *, struct error *,
    const struct config *);
static int	filediff(const struct buffer *, const struct buffer *,
    const char *);
static int	filewrite(const struct buffer *, const struct buffer *,
    const char *);
static int	fileattr(const char *, const char *);

static int	tmpfd(const char *, char *, size_t);

int
main(int argc, char *argv[])
{
	struct file_list files;
	struct config cf;
	struct error er;
	const struct file *fe;
	int error = 0;
	int ch;

	if (pledge("stdio rpath wpath cpath fattr chown proc exec", NULL) == -1)
		err(1, "pledge");

	TAILQ_INIT(&files);
	config_init(&cf);

	while ((ch = getopt(argc, argv, "Ddisv")) != -1) {
		switch (ch) {
		case 'D':
			cf.cf_flags |= CONFIG_FLAG_DIFFPARSE;
			break;
		case 'd':
			cf.cf_flags |= CONFIG_FLAG_DIFF;
			break;
		case 'i':
			cf.cf_flags |= CONFIG_FLAG_INPLACE;
			break;
		case 's':
			cf.cf_flags |= CONFIG_FLAG_SIMPLE;
			break;
		case 'v':
			cf.cf_verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((cf.cf_flags & CONFIG_FLAG_DIFFPARSE) && argc > 0)
		usage();

	if (cf.cf_flags & CONFIG_FLAG_DIFF) {
		if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
			err(1, "pledge");
	} else if (cf.cf_flags & CONFIG_FLAG_INPLACE) {
		if (pledge("stdio rpath wpath cpath fattr chown", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
	}

	error_init(&er, &cf);
	diff_init();
	lexer_init();

	if (filelist(argc, argv, &files, &cf)) {
		error = 1;
		goto out;
	}
	TAILQ_FOREACH(fe, &files, fe_entry) {
		if (fileformat(fe, &er, &cf)) {
			error = 1;
			error_flush(&er);
		}
		error_reset(&er);
	}

out:
	lexer_shutdown();
	diff_shutdown();
	error_close(&er);
	files_free(&files);

	return error;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: knfmt [-Ddis] [file ...]\n");
	exit(1);
}

static int
filelist(int argc, char **argv, struct file_list *files,
    const struct config *cf)
{
	struct file *fe;

	if (cf->cf_flags & CONFIG_FLAG_DIFFPARSE)
		return diff_parse(files, cf);

	if (argc == 0) {
		fe = file_alloc("/dev/stdin", 0);
		TAILQ_INSERT_TAIL(files, fe, fe_entry);
	} else {
		int i;

		for (i = 0; i < argc; i++) {
			fe = file_alloc(argv[i], 0);
			TAILQ_INSERT_TAIL(files, fe, fe_entry);
		}
	}
	return 0;
}

static int
fileformat(const struct file *fe, struct error *er, const struct config *cf)
{
	const struct buffer *dst, *src;
	struct parser *pr;
	int error = 0;

	pr = parser_alloc(fe, er, cf);
	if (pr == NULL) {
		error = 1;
		goto out;
	}
	dst = parser_exec(pr);
	if (dst == NULL) {
		error = 1;
		goto out;
	}

	src = lexer_get_buffer(parser_get_lexer(pr));
	if (cf->cf_flags & CONFIG_FLAG_DIFF)
		error = filediff(src, dst, fe->fe_path);
	else if (cf->cf_flags & CONFIG_FLAG_INPLACE)
		error = filewrite(src, dst, fe->fe_path);
	else
		printf("%s", dst->bf_ptr);

out:
	parser_free(pr);
	return error;
}

static int
filediff(const struct buffer *src, const struct buffer *dst, const char *path)
{
	char dstpath[PATH_MAX], srcpath[PATH_MAX];
	pid_t pid;
	int dstfd = -1;
	int error = 1;
	int srcfd = -1;
	int status;

	if (buffer_cmp(src, dst) == 0)
		return 0;

	srcfd = tmpfd(src->bf_ptr, srcpath, sizeof(srcpath));
	if (srcfd == -1)
		goto out;
	dstfd = tmpfd(dst->bf_ptr, dstpath, sizeof(dstpath));
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

	if (waitpid(pid, &status, 0) == -1) {
		warn("waitpid");
		goto out;
	}
	if (WIFSIGNALED(status))
		error = WTERMSIG(status);
	if (WIFEXITED(status))
		error = WEXITSTATUS(status);

out:
	if (srcfd != -1)
		close(srcfd);
	if (dstfd != -1)
		close(dstfd);
	return error;
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
	len = strlen(buf);
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
tmpfd(const char *buf, char *path, size_t pathsiz)
{
	char tmppath[PATH_MAX];
	ssize_t siz = sizeof(tmppath);
	size_t len;
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

	len = strlen(buf);
	while (len > 0) {
		ssize_t nw;

		nw = write(fd, buf, len);
		if (nw == -1) {
			warn("write");
			goto err;
		}
		buf += nw;
		len -= nw;
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

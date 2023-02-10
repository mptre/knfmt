#include "diff.h"

#include "config.h"

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alloc.h"
#include "buffer.h"
#include "error.h"
#include "file.h"
#include "options.h"
#include "util.h"
#include "vector.h"

struct reader {
	char	*buf;
	size_t	 off;
};

static void	diff_end(struct diffchunk *, unsigned int);

static int	matchpath(const char *, char *, size_t);
static int	matchchunk(const char *, unsigned int *, unsigned int *);
static int	matchline(const char *, unsigned int, struct file *);

static struct reader	*reader_open(const char *);
static const char	*reader_getline(struct reader *);
static void		 reader_close(struct reader *);

static const char	*trimprefix(const char *, size_t *);

static void	diff_trace(const char *, ...)
	__attribute__((__format__(printf, 1, 2)));

static regex_t	rechunk, repath;

/*
 * Number of directories above the current working directory where a Git
 * repository resides. Used to adjust diff paths when invoking knfmt from a
 * directory nested below the repository root.
 */
static int	git_ndirs;

void
diff_init(void)
{
	struct {
		regex_t		*r;
		const char	*p;
	} patterns[] = {
		{ &rechunk,	"^@@.+\\+([[:digit:]]+)(,([[:digit:]]+))?.+@@" },
		{ &repath,	"^\\+\\+\\+[[:space:]]+([^[:space:]]+)" },
	};
	size_t n = sizeof(patterns) / sizeof(patterns[0]);
	size_t i;
	int fd;

	for (i = 0; i < n; i++) {
		char errbuf[128];
		int error;

		error = regcomp(patterns[i].r, patterns[i].p,
		    REG_EXTENDED | REG_NEWLINE);
		if (error == 0)
			continue;

		if (regerror(error, patterns[i].r, errbuf, sizeof(errbuf)) > 0)
			errx(1, "regcomp: %s", errbuf);
	}

	fd = searchpath(".git", &git_ndirs);
	if (fd != -1)
		close(fd);
}

void
diff_shutdown(void)
{
	regfree(&rechunk);
	regfree(&repath);
}

int
diff_parse(struct files *files, const struct options *op)
{
	struct file *fe = NULL;
	struct reader *rd;
	const char *line;
	int error = 0;

	rd = reader_open("/dev/stdin");
	if (rd == NULL)
		return 1;

	while ((line = reader_getline(rd)) != NULL) {
		char path[PATH_MAX];
		unsigned int el, sl;

		if (matchpath(line, path, sizeof(path))) {
			fe = files_alloc(files, path, op);
		} else if (matchchunk(line, &sl, &el)) {
			/* Chunks cannot be present before the path. */
			if (fe == NULL) {
				error = 1;
				goto out;
			}

			while (sl <= el) {
				line = reader_getline(rd);
				if (line == NULL) {
					error = 1;
					goto out;
				}

				if (matchline(line, sl, fe))
					sl++;
			}

			diff_end(fe->fe_diff, el);
		}
	}

	if (trace(op, 'D')) {
		size_t i;

		for (i = 0; i < VECTOR_LENGTH(files->fs_vc); i++) {
			size_t j;

			fe = &files->fs_vc[i];
			diff_trace("%s:", fe->fe_path);
			for (j = 0; j < VECTOR_LENGTH(fe->fe_diff); j++) {
				const struct diffchunk *du = &fe->fe_diff[j];

				diff_trace("  %u-%u", du->du_beg, du->du_end);
			}
		}
	}

out:
	reader_close(rd);
	return error;
}

const struct diffchunk *
diff_get_chunk(const struct diffchunk *chunks, unsigned int lno)
{
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(chunks); i++) {
		const struct diffchunk *du = &chunks[i];

		if (lno >= du->du_beg && lno <= du->du_end)
			return du;
	}
	return NULL;
}

static void
diff_end(struct diffchunk *chunks, unsigned int lno)
{
	struct diffchunk *du;

	du = VECTOR_LAST(chunks);
	if (du != NULL && du->du_end == 0)
		du->du_end = lno;
}

static int
matchpath(const char *str, char *path, size_t pathsiz)
{
	regmatch_t rm[2];
	const char *buf;
	size_t len;
	int i;

	if (regexec(&repath, str, 2, rm, 0))
		return 0;

	buf = str + rm[1].rm_so;
	len = (size_t)(rm[1].rm_eo - rm[1].rm_so);
	if (strncmp(buf, "b/", 2) == 0) {
		/* Trim git prefix. */
		buf = trimprefix(buf, &len);
	}

	for (i = 0; i < 2; i++) {
		struct stat sb;
		int n;

		n = snprintf(path, pathsiz, "%.*s", (int)len, buf);
		if (n < 0 || (size_t)n >= pathsiz)
			errx(1, "%.*s: path too long", (int)len, buf);

		/* Try to adjust Git repository relative path(s). */
		if (git_ndirs > 0 && path[0] != '/' &&
		    stat(path, &sb) == -1 && errno == ENOENT) {
			int ntrim;

			for (ntrim = git_ndirs; ntrim > 0; ntrim--)
				buf = trimprefix(buf, &len);
		} else {
			break;
		}
	}
	return 1;
}

static int
matchchunk(const char *str, unsigned int *sl, unsigned int *el)
{
	regmatch_t rm[4];
	long n;

	if (regexec(&rechunk, str, 4, rm, 0))
		return 0;

	errno = 0;
	n = strtol(str + rm[1].rm_so, NULL, 10);
	if (n == 0 || errno)
		return 0;
	*sl = n;

	if (rm[3].rm_so != -1 && rm[3].rm_eo != -1) {
		errno = 0;
		n = strtol(str + rm[3].rm_so, NULL, 10);
		if (n == 0 || errno != 0)
			return 0;
		*el = (*sl + n) - 1;
	} else {
		*el = *sl;
	}

	return 1;
}

static int
matchline(const char *str, unsigned int lno, struct file *fe)
{
	struct diffchunk *du;

	if (str[0] == '-')
		return 0;

	du = VECTOR_LAST(fe->fe_diff);
	if (str[0] == '+') {
		if (du == NULL || (du->du_beg > 0 && du->du_end > 0)) {
			du = VECTOR_CALLOC(fe->fe_diff);
			if (du == NULL)
				err(1, NULL);
			du->du_beg = lno;
		}
	} else {
		diff_end(fe->fe_diff, lno - 1);
	}
	return 1;
}

static struct reader *
reader_open(const char *path)
{
	struct buffer *bf;
	struct reader *rd;

	bf = buffer_read(path);
	if (bf == NULL) {
		warn("%s", path);
		return NULL;
	}

	rd = ecalloc(1, sizeof(*rd));
	rd->buf = buffer_str(bf);
	buffer_free(bf);
	return rd;
}

static const char *
reader_getline(struct reader *rd)
{
	const char *line;
	char *p;
	size_t len;

	line = &rd->buf[rd->off];
	p = strchr(line, '\n');
	if (p == NULL)
		return NULL;
	*p = '\0';
	len = (size_t)(p - line) + 1;
	rd->off += len;
	return line;
}

static void
reader_close(struct reader *rd)
{
	free(rd->buf);
	free(rd);
}

static const char *
trimprefix(const char *str, size_t *len)
{
	const char *p;

	p = memchr(str, '/', *len);
	if (p == NULL)
		return str;
	*len -= (size_t)((p - str) + 1);
	return &p[1];
}

static void
diff_trace(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[I] ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
}

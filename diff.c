#include "diff.h"

#include "config.h"

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>	/* PATH_MAX */
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/vector.h"

#include "file.h"
#include "fs.h"
#include "options.h"
#include "trace-types.h"
#include "trace.h"

static void	diff_end(struct diffchunk *, unsigned int);

static int	matchpath(const char *, const char **, struct arena_scope *);
static int	matchchunk(const char *, unsigned int *, unsigned int *);
static int	matchline(const char *, unsigned int, struct file *);

static const char	*trimprefix(const char *, size_t *);

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
diff_parse(struct files *files, struct arena_scope *eternal_scope,
    struct arena *scratch, const struct options *op)
{
	struct buffer *bf;
	struct buffer_getline it = {0};
	struct file *fe = NULL;
	const char *line;
	int error = 0;

	arena_scope(scratch, s);

	bf = arena_buffer_read(&s, "/dev/stdin");
	if (bf == NULL)
		return 1;

	while ((line = arena_buffer_getline(&s, bf, &it)) != NULL) {
		const char *path;
		unsigned int el, sl;

		if (matchpath(line, &path, &s)) {
			fe = files_alloc(files, path, eternal_scope);
		} else if (matchchunk(line, &sl, &el)) {
			/* Chunks cannot be present before the path. */
			if (fe == NULL) {
				error = 1;
				goto out;
			}

			while (sl <= el) {
				line = arena_buffer_getline(&s, bf, &it);
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

	if (options_trace_level(op, TRACE_DIFF) > 0) {
		size_t i;

		for (i = 0; i < VECTOR_LENGTH(files->fs_vc); i++) {
			size_t j;

			fe = &files->fs_vc[i];
			for (j = 0; j < VECTOR_LENGTH(fe->fe_diff); j++) {
				const struct diffchunk *du = &fe->fe_diff[j];

				trace(TRACE_DIFF, op, "%s: %u-%u",
				    fe->fe_path, du->du_beg, du->du_end);
			}
		}
	}

out:
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
matchpath(const char *str, const char **out, struct arena_scope *s)
{
	regmatch_t rm[2];
	struct stat sb;
	const char *buf, *path;
	size_t len;

	if (regexec(&repath, str, 2, rm, 0))
		return 0;

	buf = str + rm[1].rm_so;
	len = (size_t)(rm[1].rm_eo - rm[1].rm_so);
	if (strncmp(buf, "b/", 2) == 0) {
		/* Trim git prefix. */
		buf = trimprefix(buf, &len);
	}
	path = arena_strndup(s, buf, len);

	/* Try to adjust Git repository relative path(s). */
	if (git_ndirs > 0 && path[0] != '/' &&
	    stat(path, &sb) == -1 && errno == ENOENT) {
		int ntrim;

		for (ntrim = git_ndirs; ntrim > 0; ntrim--)
			buf = trimprefix(buf, &len);
		path = arena_strndup(s, buf, len);
	}

	*out = path;
	return 1;
}

static unsigned int
strtou(const char *str)
{
	unsigned long n;

	errno = 0;
	n = strtoul(str, NULL, 10);
	if (n == 0 || n > UINT_MAX || errno != 0)
		return 0;
	return (unsigned int)n;
}

static int
matchchunk(const char *str, unsigned int *sl, unsigned int *el)
{
	regmatch_t rm[4];
	unsigned int lno;

	if (regexec(&rechunk, str, 4, rm, 0))
		return 0;

	lno = strtou(str + rm[1].rm_so);
	if (lno == 0)
		return 0;
	*sl = lno;

	if (rm[3].rm_so != -1 && rm[3].rm_eo != -1) {
		lno = strtou(str + rm[3].rm_so);
		if (lno == 0)
			return 0;
		*el = (*sl + lno) - 1;
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

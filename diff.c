#include <err.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static void	diff_end(struct diff *, unsigned int);

static int	matchpath(char *, char **);
static int	matchchunk(char *, int *, int *);
static int	matchline(char *, int, struct file *);

static char	*skipline(char *);

static int	xregexec(const regex_t *, char *, regmatch_t *, size_t);

static void	diff_trace(const char *, ...)
	__attribute__((__format__(printf, 1, 2)));

static regex_t	rechunk, repath;

void
files_free(struct file_list *files)
{
	struct file *fe;

	while ((fe = TAILQ_FIRST(files)) != NULL) {
		TAILQ_REMOVE(files, fe, fe_entry);
		file_free(fe);
	}
}

struct file *
file_alloc(char *path, unsigned int flags)
{
	struct file *fe;

	fe = calloc(1, sizeof(*fe));
	if (fe == NULL)
		err(1, NULL);
	fe->fe_path = path;
	fe->fe_flags = flags;
	TAILQ_INIT(&fe->fe_diff.di_chunks);
	return fe;
}

void
file_free(struct file *fe)
{
	struct diffchunk *du;

	if (fe == NULL)
		return;

	while ((du = TAILQ_FIRST(&fe->fe_diff.di_chunks)) != NULL) {
		TAILQ_REMOVE(&fe->fe_diff.di_chunks, du, du_entry);
		free(du);
	}

	if (fe->fe_flags & FILE_FLAG_FREE)
		free(fe->fe_path);
	free(fe);
}

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
}

void
diff_shutdown(void)
{
	regfree(&rechunk);
	regfree(&repath);
}

int
diff_parse(struct file_list *files, const struct config *cf)
{
	struct file *fe = NULL;
	char *buf;
	struct buffer *bf;

	bf = buffer_read("/dev/stdin");
	if (bf == NULL)
		return 1;
	buffer_appendc(bf, '\0');
	buf = bf->bf_ptr;

	for (;;) {
		char *path;
		int el, sl;

		if (matchpath(buf, &path)) {
			fe = file_alloc(path, FILE_FLAG_FREE);
			TAILQ_INSERT_TAIL(files, fe, fe_entry);

			buf = skipline(buf);
			if (buf == NULL)
				goto err;
		} else if (matchchunk(buf, &sl, &el)) {
			/* Chunks cannot be present before the path. */
			if (fe == NULL)
				goto err;

			buf = skipline(buf);
			if (buf == NULL)
				goto err;

			while (sl <= el) {
				if (matchline(buf, sl, fe))
					sl++;

				buf = skipline(buf);
				if (buf == NULL)
					goto err;
			}

			diff_end(&fe->fe_diff, el);
		} else {
			buf = skipline(buf);
		}

		if (buf == NULL)
			break;
	}

	if (TRACE(cf)) {
		TAILQ_FOREACH(fe, files, fe_entry) {
			const struct diffchunk *du;

			diff_trace("%s:", fe->fe_path);
			TAILQ_FOREACH(du, &fe->fe_diff.di_chunks, du_entry) {
				diff_trace("  %u-%u", du->du_beg, du->du_end);
			}
		}
	}

	buffer_free(bf);
	return 0;

err:
	buffer_free(bf);
	files_free(files);
	return 1;
}

int
diff_covers(const struct diff *di, unsigned int lno)
{
	const struct diffchunk *du;

	TAILQ_FOREACH(du, &di->di_chunks, du_entry) {
		if (lno >= du->du_beg && lno <= du->du_end)
			return 1;
	}
	return 0;
}

static void
diff_end(struct diff *di, unsigned int lno)
{
	struct diffchunk *du;

	du = TAILQ_LAST(&di->di_chunks, diffchunk_list);
	if (du != NULL && du->du_end == 0)
		du->du_end = lno;
}

static int
matchpath(char *str, char **path)
{
	regmatch_t rm[2];
	const char *buf;
	size_t len;

	if (xregexec(&repath, str, rm, 2))
		return 0;

	buf = str + rm[1].rm_so;
	len = rm[1].rm_eo - rm[1].rm_so;
	if (strncmp(buf, "b/", 2) == 0) {
		/* Trim git prefix. */
		buf += 2;
		len -= 2;
	}
	*path = strndup(buf, len);
	if (*path == NULL)
		err(1, NULL);
	return 1;
}

static int
matchchunk(char *str, int *sl, int *el)
{
	regmatch_t rm[4];
	long n;

	if (xregexec(&rechunk, str, rm, 4))
		return 0;

	errno = 0;
	n = strtol(str + rm[1].rm_so, NULL, 10);
	if (n == 0 || errno)
		return 0;
	*sl = n;

	if (rm[3].rm_so != -1 && rm[3].rm_eo != -1) {
		errno = 0;
		n = strtol(str + rm[3].rm_so, NULL, 10);
		if (n == 0 || errno)
			return 0;
		*el = (*sl + n) - 1;
	} else {
		*el = *sl;
	}

	return 1;
}

static int
matchline(char *str, int lno, struct file *fe)
{
	struct diffchunk *du;

	if (str[0] == '-')
		return 0;

	du = TAILQ_LAST(&fe->fe_diff.di_chunks, diffchunk_list);
	if (str[0] == '+') {
		if (du == NULL || (du->du_beg > 0 && du->du_end > 0)) {
			du = calloc(1, sizeof(*du));
			if (du == NULL)
				err(1, NULL);
			du->du_beg = lno;
			TAILQ_INSERT_TAIL(&fe->fe_diff.di_chunks, du, du_entry);
		}
	} else {
		diff_end(&fe->fe_diff, lno - 1);
	}
	return 1;
}

static char *
skipline(char *str)
{
	char *p;

	p = strchr(str, '\n');
	if (p == NULL)
		return NULL;
	return p + 1;
}

static int
xregexec(const regex_t *re, char *str, regmatch_t *rm, size_t n)
{
	char *end;
	int error;

	end = skipline(str);
	if (end == NULL)
		return REG_NOMATCH;
	/* Ugly, operate on a per line basis. */
	end[-1] = '\0';
	error = regexec(re, str, n, rm, 0);
	end[-1] = '\n';
	return error;
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

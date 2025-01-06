#include "config.h"

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/exec.h"
#include "libks/fs.h"
#include "libks/vector.h"

#include "clang.h"
#include "diff.h"
#include "expr.h"
#include "file.h"
#include "lexer.h"
#include "options.h"
#include "parser.h"
#include "simple.h"
#include "style.h"
#include "trace-types.h"

struct main_context {
	struct options	 options;
	struct style	*style;
	struct simple	*simple;
	struct buffer	*src;
	struct buffer	*dst;

	struct {
		struct arena		*eternal;
		struct arena		*scratch;
		struct arena		*doc;
		struct arena		*buffer;
		struct arena		*ruler;
	} arena;
};

static void	usage(void) __attribute__((noreturn));

static int	filelist(int, char **, struct files *, struct arena_scope *,
    struct arena *, const struct options *);
static int	fileformat(struct main_context *, struct file *);
static int	filediff(struct main_context *, const struct file *);
static int	filewrite(struct main_context *, const struct file *);
static int	fileprint(const struct buffer *);

int
main(int argc, char *argv[])
{
	struct main_context c = {0};
	struct files files = {0};
	const char *clang_format = NULL;
	size_t i;
	int error = 0;
	int ch;

	if (pledge("stdio rpath wpath cpath fattr chown proc exec", NULL) == -1)
		err(1, "pledge");

	options_init(&c.options);

	while ((ch = getopt(argc, argv, "c:Ddist:")) != -1) {
		switch (ch) {
		case 'c':
			clang_format = optarg;
			break;
		case 'D':
			c.options.diffparse = 1;
			break;
		case 'd':
			c.options.diff = 1;
			break;
		case 'i':
			c.options.inplace = 1;
			break;
		case 's':
			c.options.simple = 1;
			break;
		case 't':
			if (options_trace_parse(&c.options, optarg))
				return 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((c.options.diffparse && argc > 0) ||
	    (!c.options.diffparse && c.options.inplace && argc == 0))
		usage();

	clang_init();
	expr_init();
	style_init();
	if (c.options.diffparse)
		diff_init();

	if (VECTOR_INIT(files.fs_vc))
		err(1, NULL);

	c.arena.eternal = arena_alloc();
	arena_scope(c.arena.eternal, eternal_scope);
	c.arena.scratch = arena_alloc();
	c.arena.doc = arena_alloc();
	c.arena.buffer = arena_alloc();
	c.arena.ruler = arena_alloc();
	arena_scope(c.arena.buffer, buffer_scope);
	c.style = style_parse(clang_format, &eternal_scope, c.arena.scratch,
	    &c.options);
	if (c.style == NULL) {
		error = 1;
		goto out;
	}

	if (c.options.diff) {
		if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
			err(1, "pledge");
	} else if (c.options.inplace) {
		if (pledge("stdio rpath wpath cpath fattr chown", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
	}

	c.simple = simple_alloc(&eternal_scope, &c.options);

	c.src = arena_buffer_alloc(&buffer_scope, 1 << 12);
	c.dst = arena_buffer_alloc(&buffer_scope, 1 << 12);

	if (filelist(argc, argv, &files, &eternal_scope, c.arena.scratch,
	    &c.options)) {
		error = 1;
		goto out;
	}

	for (i = 0; i < VECTOR_LENGTH(files.fs_vc); i++) {
		struct file *fe = &files.fs_vc[i];

		if (fileformat(&c, fe))
			error = 1;
		buffer_reset(c.src);
		buffer_reset(c.dst);
		file_close(fe);
	}

out:
	files_free(&files);
	arena_free(c.arena.ruler);
	arena_free(c.arena.buffer);
	arena_free(c.arena.doc);
	arena_free(c.arena.scratch);
	arena_free(c.arena.eternal);
	style_shutdown();
	expr_shutdown();
	clang_shutdown();
	if (c.options.diffparse)
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
    struct arena_scope *eternal_scope, struct arena *scratch,
    const struct options *op)
{
	if (op->diffparse)
		return diff_parse(files, eternal_scope, scratch, op);

	if (argc == 0) {
		files_alloc(files, "/dev/stdin", eternal_scope);
	} else {
		int i;

		for (i = 0; i < argc; i++)
			files_alloc(files, argv[i], eternal_scope);
	}
	return 0;
}

static int
fileformat(struct main_context *c, struct file *fe)
{
	struct clang *clang;
	struct lexer *lx = NULL;
	struct parser *pr = NULL;

	arena_scope(c->arena.eternal, eternal_scope);

	if (file_read(fe, c->src))
		return 1;
	clang = clang_alloc(c->style, c->simple, &eternal_scope,
	    c->arena.scratch, c->arena.ruler, &c->options);
	lx = lexer_tokenize(&(const struct lexer_arg){
	    .path		= fe->fe_path,
	    .bf			= c->src,
	    .diff		= fe->fe_diff,
	    .op			= &c->options,
	    .error_flush	= options_trace_level(&c->options,
		TRACE_LEXER) > 0,
	    .arena		= {
		.eternal_scope	= &eternal_scope,
		.scratch	= c->arena.scratch,
	    },
	    .callbacks		= clang_lexer_callbacks(clang),
	});
	if (lx == NULL)
		return 1;
	pr = parser_alloc(&(struct parser_arg){
	    .lexer	= lx,
	    .options	= &c->options,
	    .style	= c->style,
	    .simple	= c->simple,
	    .clang	= clang,
	    .arena	= {
		.eternal_scope	= &eternal_scope,
		.scratch	= c->arena.scratch,
		.doc		= c->arena.doc,
		.buffer		= c->arena.buffer,
		.ruler		= c->arena.ruler,
	    },
	});
	if (parser_exec(pr, fe->fe_diff, c->dst))
		return 1;

	if (c->options.diff)
		return filediff(c, fe);
	if (c->options.inplace)
		return filewrite(c, fe);
	return fileprint(c->dst);
}

static int
filediff(struct main_context *c, const struct file *fe)
{
	int error;

	if (buffer_cmp(c->src, c->dst) == 0)
		return 0;

	error = KS_exec_diff(fe->fe_path,
	    buffer_get_ptr(c->src), buffer_get_len(c->src),
	    buffer_get_ptr(c->dst), buffer_get_len(c->dst));
	if (error == -1)
		warn("%s", fe->fe_path);
	return error;
}

static int
filewrite(struct main_context *c, const struct file *fe)
{
	const struct buffer *src = c->src;
	const struct buffer *dst = c->dst;

	if (buffer_cmp(src, dst) == 0)
		return 0;
	if (KS_fs_replace(fe->fe_path,
	    buffer_get_ptr(dst), buffer_get_len(dst)) == -1) {
		warn("%s", fe->fe_path);
		return 1;
	}
	return 0;
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

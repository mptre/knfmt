#include "parser-extern.h"

#include "buffer.h"
#include "error.h"
#include "lexer.h"
#include "options.h"

static int	parser_get_error(const struct parser_context *);

int
parser_good(const struct parser_context *pc)
{
	if (lexer_is_branch(pc->pc_lx))
		return BRCH;
	return parser_get_error(pc) ? FAIL : GOOD;
}

int
parser_none(const struct parser_context *pc)
{
	if (lexer_is_branch(pc->pc_lx))
		return BRCH;
	return parser_get_error(pc) ? FAIL : NONE;
}

int
parser_fail0(struct parser_context *pc, const char *fun, int lno)
{
	struct error *er = pc->pc_er;
	struct buffer *bf;
	struct token *tk;

	if (parser_get_error(pc))
		goto out;
	pc->pc_error = 1;

	bf = error_begin(er);
	buffer_printf(bf, "%s: ", pc->pc_path);
	if (trace(pc->pc_op, 'l'))
		buffer_printf(bf, "%s:%d: ", fun, lno);
	buffer_printf(bf, "error at ");
	if (lexer_back(pc->pc_lx, &tk))
		buffer_printf(bf, "%s\n", lexer_serialize(pc->pc_lx, tk));
	else
		buffer_printf(bf, "(null)\n");
	error_end(er);

out:
	if (lexer_is_branch(pc->pc_lx))
		return BRCH;
	return FAIL;
}

void
parser_reset(struct parser_context *pc)
{
	error_reset(pc->pc_er);
	pc->pc_error = 0;
}

static int
parser_get_error(const struct parser_context *pc)
{
	return pc->pc_error || lexer_get_error(pc->pc_lx);
}

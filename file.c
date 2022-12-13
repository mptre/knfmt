#include "file.h"

#include "config.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "diff.h"
#include "error.h"
#include "options.h"
#include "vector.h"

struct file *
files_alloc(struct files *files, const char *path, const struct options *op)
{
	struct file *fe;

	fe = VECTOR_CALLOC(files->fs_vc);
	if (fe == NULL)
		err(1, NULL);
	fe->fe_path = estrdup(path);
	if (VECTOR_INIT(fe->fe_diff) == NULL)
		err(1, NULL);
	fe->fe_error = error_alloc(trace(op, 'l') >= 2);
	return fe;
}

void
files_free(struct files *files)
{
	while (!VECTOR_EMPTY(files->fs_vc)) {
		struct file *fe;

		fe = VECTOR_POP(files->fs_vc);
		VECTOR_FREE(fe->fe_diff);
		free(fe->fe_path);
		error_free(fe->fe_error);
	}
	VECTOR_FREE(files->fs_vc);
}

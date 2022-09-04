#include "file.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "diff.h"
#include "error.h"
#include "extern.h"
#include "options.h"
#include "vector.h"

struct file *
files_alloc(struct files *files, const char *path, const struct options *op)
{
	struct file *fe;

	fe = VECTOR_CALLOC(files->fs_vc);
	if (fe == NULL)
		err(1, NULL);
	fe->fe_path = strdup(path);
	if (fe->fe_path == NULL)
		err(1, NULL);
	if (VECTOR_INIT(fe->fe_diff) == NULL)
		err(1, NULL);
	fe->fe_error = error_alloc(options_trace(op));
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

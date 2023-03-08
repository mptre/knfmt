#include "file.h"

#include "config.h"

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alloc.h"
#include "buffer.h"
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
	fe->fe_fd = -1;
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
		file_close(fe);
	}
	VECTOR_FREE(files->fs_vc);
}

struct buffer *
file_read(struct file *fe)
{
	struct buffer *bf;
	int fd;

	fd = open(fe->fe_path, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		goto err;
	bf = buffer_read_fd(fd);
	if (bf == NULL)
		goto err;
	fe->fe_fd = fd;
	return bf;

err:
	warn("%s", fe->fe_path);
	if (fd != -1)
		close(fd);
	return NULL;
}

void
file_close(struct file *fe)
{
	if (fe->fe_fd == -1)
		return;
	close(fe->fe_fd);
	fe->fe_fd = -1;
}

#include "file.h"

#include "config.h"

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/vector.h"

#include "diff.h"

struct file *
files_alloc(struct files *files, const char *path,
    struct arena_scope *eternal_scope)
{
	struct file *fe;

	fe = VECTOR_CALLOC(files->fs_vc);
	if (fe == NULL)
		err(1, NULL);
	fe->fe_path = arena_strdup(eternal_scope, path);
	if (VECTOR_INIT(fe->fe_diff))
		err(1, NULL);
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
		file_close(fe);
	}
	VECTOR_FREE(files->fs_vc);
}

int
file_read(struct file *fe, struct buffer *bf)
{
	int fd;

	fd = open(fe->fe_path, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		goto err;
	if (buffer_read_fd_impl(bf, fd))
		goto err;
	fe->fe_fd = fd;
	return 0;

err:
	warn("%s", fe->fe_path);
	if (fd != -1)
		close(fd);
	return 1;
}

void
file_close(struct file *fe)
{
	if (fe->fe_fd == -1)
		return;
	close(fe->fe_fd);
	fe->fe_fd = -1;
}

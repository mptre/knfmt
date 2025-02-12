#include "path.h"

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

/*
 * Search for the given filename starting at the current working directory and
 * traverse upwards. Returns a file descriptor to the file if found, -1 otherwise.
 * The optional nlevels argument reflects the number of directories traversed
 * upwards.
 */
int
searchpath(const char *filename, int *nlevels)
{
	struct stat sb;
	dev_t dev = 0;
	ino_t ino = 0;
	int fd = -1;
	int flags = O_RDONLY | O_CLOEXEC;
	int i = 0;
	int dirfd;

	dirfd = open(".", flags | O_DIRECTORY);
	if (dirfd == -1)
		return -1;
	if (fstat(dirfd, &sb) == -1)
		goto out;
	dev = sb.st_dev;
	ino = sb.st_ino;
	for (;;) {
		fd = openat(dirfd, filename, flags);
		if (fd >= 0)
			break;

		fd = openat(dirfd, "..", flags | O_DIRECTORY);
		close(dirfd);
		dirfd = fd;
		fd = -1;
		if (dirfd == -1)
			break;
		if (fstat(dirfd, &sb) == -1)
			break;
		if (dev == sb.st_dev && ino == sb.st_ino)
			break;
		dev = sb.st_dev;
		ino = sb.st_ino;
		i++;
	}

out:
	if (dirfd != -1)
		close(dirfd);
	if (fd != -1 && nlevels != NULL)
		*nlevels = i;
	return fd;
}

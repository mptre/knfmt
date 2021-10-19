/*
 * Switch case scope with break outside.
 */

int
main(void)
{
	switch (uio->uio_segflg) {
	case UIO_USERSPACE: {
		char tmp = c;

		if (copyout(&tmp, iov->iov_base, sizeof(char)) != 0)
			return (EFAULT);
	}
		break;
	}
}

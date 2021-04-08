/*
 * Nested if/else under switch.
 */

int
main(void)
{
	switch (cmd) {
	case FIOASYNC:
		if (*(int *)data) {
			mpipe->pipe_state |= PIPE_ASYNC;
		} else {
			mpipe->pipe_state &= ~PIPE_ASYNC;
		}
		break;
	}
}

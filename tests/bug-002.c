/*
 * Non-optimal break of long expression.
 */

int
main(void)
{
	if (kd_buf_size < kd_len /* overflow? */
	    || (SIZE_MAX / kd_len ) < kd_len
	    || kd_state_size > algo_config->permitted_state_size) {
		return;
	}
}

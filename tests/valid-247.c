/*
 * No hard line before function annotation.
 */

uint32_t
arc4random(void)
{
	uint32_t val;

	_ARC4_LOCK();
	_rs_random_u32(&val);
	_ARC4_UNLOCK();
	return val;
}
DEF_WEAK(arc4random);

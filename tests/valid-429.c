/*
 * Function argument type wrapped in cpp macro.
 */

static uint32_t
foo1(VECTOR(const Foo) x)
{
	return VECTOR_LENGTH(x);
}

static uint32_t
foo2(VECTOR(int) x)
{
	return VECTOR_LENGTH(x);
}

static uint32_t
foo2(VECTOR(const int) x)
{
	return VECTOR_LENGTH(x);
}

static uint32_t
foo2(VECTOR(const volatile int) x)
{
	return VECTOR_LENGTH(x);
}

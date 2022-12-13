/*
 * Invalid brace initializer making use of cpp.
 */

struct foo a = {
	INIT(,),
};

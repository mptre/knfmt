#ifndef _TOKEN_TYPE_H_
#define _TOKEN_TYPE_H_

enum token_type {
#define T(t, s, f) t,
#define S(t, s, f) t,
#include "token-defs.h"
};

#endif

enum token_type {
#define T(t, s, f) t,
#define S(t, s, f) t,
#include "token.h"
};

/*
 * Group variable declarations.
 */

struct a {
	enum {
		b,
	} c;
};

struct token	*tk, *pv;

int x, a;
int z = 1, y;
int b = 1, c, d = 2;
int comment; /* comment */
int comment /* comment */, c1;
int comment /* comment */, c2, comment /* comment */;

struct x	*f;
struct x	 a;

int c, i, rflags = REG_EXTENDED;

int a = 1;
int b = 2;
int c;

int a = 1;
char b;
int b;

unsigned int u1 = 0;
unsigned int u2;

char *v;
char *x;

struct x {
	int x;
	int y;
};

int ret = 0;
int i, j,k, m,n, again, bufsize;
unsigned char *s = NULL, *sp;
unsigned char *bufp;
int first = 1;
size_t num = 0, slen = 0;

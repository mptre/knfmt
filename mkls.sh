mkls -s "$@" -- \
SRCS		!(fuzz-*|knfmt|t).c $(cd libks && ls *.c) -- \
KNFMT		!(compat-*).c compat-pledge.c !(compat-*|config).h -- \
CLANGTIDY	!(config|compat-*).[ch] -- \
CPPCHECK	!(compat-*).c -- \
SHLINT		configure tests/*.sh

cd tests
mkls -s "$@" -- \
TESTS	diff-[0-9]*.c -- \
TESTS	diff-simple-[0-9]*.c -- \
TESTS	diff-style-[0-9]*.c -- \
TESTS	error*.c -- \
TESTS	valid*.c -- \
TESTS	simple*.c -- \
TESTS	inplace*.c -- \
TESTS	trace*.c -- \
TESTS	bug*.c -- \
TESTS	style*.c -- \
TESTS	../!(compat-*).c ../!(compat-*|config).h \
	../compat-pledge.c ../libks/*.[ch] -- \
TESTS	!(cp|knfmt).sh

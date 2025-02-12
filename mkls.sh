mkls -s "$@" -- \
SRCS		!(fuzz-*|knfmt|t).c $(cd libks && ls *.[cS]) -- \
KNFMT		!(compat-*).c compat-pledge.c !(compat-*|config).h -- \
CLANGTIDY	!(config|compat-*).[ch] -- \
CPPCHECK	!(compat-*).c -- \
IWYU		!(compat-*).c !(config).h -- \
SHLINT		configure tests/*.sh

cd tests
mkls -s "$@" -- \
TESTS	diff-[0-9]*.c -- \
TESTS	diff-simple-[0-9]*.c -- \
TESTS	diff-style-[0-9]*.c -- \
TESTS	error*.[ch] -- \
TESTS	valid*.c -- \
TESTS	simple*.c -- \
TESTS	inplace*.c -- \
TESTS	trace*.c -- \
TESTS	bug*.c -- \
TESTS	style*.[ch] -- \
TESTS	../!(compat-*).c ../!(compat-*|config).h \
	../compat-pledge.c ../libks/*.[ch] -- \
TESTS	!(cp|knfmt).sh

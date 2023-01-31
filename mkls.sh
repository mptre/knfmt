export LC_ALL=C

mkls "$@" \
SRCS \
	!(fuzz-*|knfmt|t).c \
	-- \
KNFMT \
	!(compat-*).c !(compat-*|config).h \
	compat-pledge.c \
	-- \
CLANGTIDY \
	!(config|compat-*|token-defs|vector).[ch] \
	-- \
CPPCHECK \
	!(compat-*|vector).c \
	-- \
DISTFILES \
	*.c !(config).h *.md \
	GNUmakefile LICENSE Makefile configure knfmt.1 \
	tests/*.+(c|ok|patch|sh) \
	tests/GNUmakefile tests/Makefile

cd tests
mkls "$@" \
TESTS diff-[0-9]*.c -- \
TESTS diff-simple-[0-9]*.c -- \
TESTS error*.c -- \
TESTS valid*.c -- \
TESTS simple*.c -- \
TESTS inplace*.c -- \
TESTS trace*.c -- \
TESTS bug*.c -- \
TESTS style*.c -- \
TESTS \
	../!(compat-*).c ../!(compat-*|config).h \
	../compat-pledge.c

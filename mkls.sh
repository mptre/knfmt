mkls "$@" \
SRCS \
	!(knfmt|t).c \
	-- \
KNFMT \
	!(compat-*).c !(compat-*|config).h \
	compat-pledge.c \
	-- \
DISTFILES \
	*.c !(config).h *.md \
	GNUmakefile LICENSE Makefile configure knfmt.1 \
	tests/*.+(c|ok|patch|sh) \
	tests/GNUmakefile tests/Makefile

cd tests
mkls "$@" \
TESTS diff*.c -- \
TESTS error*.c -- \
TESTS valid*.c -- \
TESTS simple*.c -- \
TESTS trace*.c -- \
TESTS bug*.c -- \
TESTS \
	../!(compat-*).c ../!(compat-*|config).h \
	../compat-pledge.c

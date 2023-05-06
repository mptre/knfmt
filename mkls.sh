export LC_ALL=C

mkls "$@" \
SRCS \
	!(fuzz-*|knfmt|t).c \
	$(find libks -type f -name '*.c' -exec basename {} \;) \
	-- \
KNFMT \
	!(compat-*).c !(compat-*|config).h \
	compat-pledge.c \
	-- \
CLANGTIDY \
	!(config|compat-*|token-defs).[ch] \
	-- \
CPPCHECK \
	!(compat-*).c \
	-- \
SHLINT \
	configure tests/*.sh \
	-- \
DISTFILES \
	*.c !(config).h libks/*.[ch] *.md \
	GNUmakefile LICENSE Makefile configure knfmt.1 \
	tests/*.+(c|ok|patch|sh) \
	tests/GNUmakefile tests/Makefile

cd tests
mkls "$@" \
TESTS diff-[0-9]*.c -- \
TESTS diff-simple-[0-9]*.c -- \
TESTS diff-style-[0-9]*.c -- \
TESTS error*.c -- \
TESTS valid*.c -- \
TESTS simple*.c -- \
TESTS inplace*.c -- \
TESTS trace*.c -- \
TESTS bug*.c -- \
TESTS style*.c -- \
TESTS \
	../!(compat-*).c ../!(compat-*|config).h \
	../compat-pledge.c \
	../libks/*.[ch]

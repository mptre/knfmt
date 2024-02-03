include ${.CURDIR}/config.mk

VERSION=	4.4.0rc3

SRCS+=	arena-buffer.c
SRCS+=	arena.c
SRCS+=	arithmetic.c
SRCS+=	buffer.c
SRCS+=	clang.c
SRCS+=	comment.c
SRCS+=	compat-pledge.c
SRCS+=	consistency.c
SRCS+=	cpp-align.c
SRCS+=	cpp-include.c
SRCS+=	diff.c
SRCS+=	doc.c
SRCS+=	error.c
SRCS+=	expr.c
SRCS+=	file.c
SRCS+=	fs.c
SRCS+=	lexer.c
SRCS+=	map.c
SRCS+=	options.c
SRCS+=	parser-attributes.c
SRCS+=	parser-braces.c
SRCS+=	parser-cpp.c
SRCS+=	parser-decl.c
SRCS+=	parser-expr.c
SRCS+=	parser-extern.c
SRCS+=	parser-func.c
SRCS+=	parser-stmt-asm.c
SRCS+=	parser-stmt-expr.c
SRCS+=	parser-stmt.c
SRCS+=	parser-type.c
SRCS+=	parser.c
SRCS+=	ruler.c
SRCS+=	simple-decl-forward.c
SRCS+=	simple-decl-proto.c
SRCS+=	simple-decl.c
SRCS+=	simple-static.c
SRCS+=	simple-stmt.c
SRCS+=	simple.c
SRCS+=	string.c
SRCS+=	style.c
SRCS+=	tmp.c
SRCS+=	token.c
SRCS+=	trace.c
SRCS+=	util.c
SRCS+=	vector.c

SRCS_knfmt+=	${SRCS}
SRCS_knfmt+=	knfmt.c
OBJS_knfmt=	${SRCS_knfmt:.c=.o}
DEPS_knfmt=	${SRCS_knfmt:.c=.d}
PROG_knfmt=	knfmt

SRCS_test+=	${SRCS}
SRCS_test+=	t.c
OBJS_test=	${SRCS_test:.c=.o}
DEPS_test=	${SRCS_test:.c=.d}
PROG_test=	t

SRCS_fuzz-style+=	${SRCS}
SRCS_fuzz-style+=	fuzz-style.c
OBJS_fuzz-style=	${SRCS_fuzz-style:.c=.o}
DEPS_fuzz-style=	${SRCS_fuzz-style:.c=.d}
PROG_fuzz-style=	fuzz-style

KNFMT+=	clang.c
KNFMT+=	clang.h
KNFMT+=	comment.c
KNFMT+=	comment.h
KNFMT+=	compat-pledge.c
KNFMT+=	cpp-align.c
KNFMT+=	cpp-align.h
KNFMT+=	cpp-include.c
KNFMT+=	cpp-include.h
KNFMT+=	diff.c
KNFMT+=	diff.h
KNFMT+=	doc.c
KNFMT+=	doc.h
KNFMT+=	error.c
KNFMT+=	error.h
KNFMT+=	expr.c
KNFMT+=	expr.h
KNFMT+=	file.c
KNFMT+=	file.h
KNFMT+=	fs.c
KNFMT+=	fs.h
KNFMT+=	fuzz-style.c
KNFMT+=	knfmt.c
KNFMT+=	lexer.c
KNFMT+=	lexer.h
KNFMT+=	options.c
KNFMT+=	options.h
KNFMT+=	parser-attributes.c
KNFMT+=	parser-attributes.h
KNFMT+=	parser-braces.c
KNFMT+=	parser-braces.h
KNFMT+=	parser-cpp.c
KNFMT+=	parser-cpp.h
KNFMT+=	parser-decl.c
KNFMT+=	parser-decl.h
KNFMT+=	parser-expr.c
KNFMT+=	parser-expr.h
KNFMT+=	parser-extern.c
KNFMT+=	parser-extern.h
KNFMT+=	parser-func.c
KNFMT+=	parser-func.h
KNFMT+=	parser-priv.h
KNFMT+=	parser-stmt-asm.c
KNFMT+=	parser-stmt-asm.h
KNFMT+=	parser-stmt-expr.c
KNFMT+=	parser-stmt-expr.h
KNFMT+=	parser-stmt.c
KNFMT+=	parser-stmt.h
KNFMT+=	parser-type.c
KNFMT+=	parser-type.h
KNFMT+=	parser.c
KNFMT+=	parser.h
KNFMT+=	queue-fwd.h
KNFMT+=	ruler.c
KNFMT+=	ruler.h
KNFMT+=	simple-decl-forward.c
KNFMT+=	simple-decl-forward.h
KNFMT+=	simple-decl-proto.c
KNFMT+=	simple-decl-proto.h
KNFMT+=	simple-decl.c
KNFMT+=	simple-decl.h
KNFMT+=	simple-static.c
KNFMT+=	simple-static.h
KNFMT+=	simple-stmt.c
KNFMT+=	simple-stmt.h
KNFMT+=	simple.c
KNFMT+=	simple.h
KNFMT+=	style.c
KNFMT+=	style.h
KNFMT+=	t.c
KNFMT+=	token.c
KNFMT+=	token.h
KNFMT+=	trace.c
KNFMT+=	trace.h
KNFMT+=	util.c
KNFMT+=	util.h

CLANGTIDY+=	clang.c
CLANGTIDY+=	clang.h
CLANGTIDY+=	comment.c
CLANGTIDY+=	comment.h
CLANGTIDY+=	cpp-align.c
CLANGTIDY+=	cpp-align.h
CLANGTIDY+=	cpp-include.c
CLANGTIDY+=	cpp-include.h
CLANGTIDY+=	diff.c
CLANGTIDY+=	diff.h
CLANGTIDY+=	doc.c
CLANGTIDY+=	doc.h
CLANGTIDY+=	error.c
CLANGTIDY+=	error.h
CLANGTIDY+=	expr.c
CLANGTIDY+=	expr.h
CLANGTIDY+=	file.c
CLANGTIDY+=	file.h
CLANGTIDY+=	fs.c
CLANGTIDY+=	fs.h
CLANGTIDY+=	fuzz-style.c
CLANGTIDY+=	knfmt.c
CLANGTIDY+=	lexer.c
CLANGTIDY+=	lexer.h
CLANGTIDY+=	options.c
CLANGTIDY+=	options.h
CLANGTIDY+=	parser-attributes.c
CLANGTIDY+=	parser-attributes.h
CLANGTIDY+=	parser-braces.c
CLANGTIDY+=	parser-braces.h
CLANGTIDY+=	parser-cpp.c
CLANGTIDY+=	parser-cpp.h
CLANGTIDY+=	parser-decl.c
CLANGTIDY+=	parser-decl.h
CLANGTIDY+=	parser-expr.c
CLANGTIDY+=	parser-expr.h
CLANGTIDY+=	parser-extern.c
CLANGTIDY+=	parser-extern.h
CLANGTIDY+=	parser-func.c
CLANGTIDY+=	parser-func.h
CLANGTIDY+=	parser-priv.h
CLANGTIDY+=	parser-stmt-asm.c
CLANGTIDY+=	parser-stmt-asm.h
CLANGTIDY+=	parser-stmt-expr.c
CLANGTIDY+=	parser-stmt-expr.h
CLANGTIDY+=	parser-stmt.c
CLANGTIDY+=	parser-stmt.h
CLANGTIDY+=	parser-type.c
CLANGTIDY+=	parser-type.h
CLANGTIDY+=	parser.c
CLANGTIDY+=	parser.h
CLANGTIDY+=	queue-fwd.h
CLANGTIDY+=	ruler.c
CLANGTIDY+=	ruler.h
CLANGTIDY+=	simple-decl-forward.c
CLANGTIDY+=	simple-decl-forward.h
CLANGTIDY+=	simple-decl-proto.c
CLANGTIDY+=	simple-decl-proto.h
CLANGTIDY+=	simple-decl.c
CLANGTIDY+=	simple-decl.h
CLANGTIDY+=	simple-static.c
CLANGTIDY+=	simple-static.h
CLANGTIDY+=	simple-stmt.c
CLANGTIDY+=	simple-stmt.h
CLANGTIDY+=	simple.c
CLANGTIDY+=	simple.h
CLANGTIDY+=	style.c
CLANGTIDY+=	style.h
CLANGTIDY+=	t.c
CLANGTIDY+=	token.c
CLANGTIDY+=	token.h
CLANGTIDY+=	trace.c
CLANGTIDY+=	trace.h
CLANGTIDY+=	util.c
CLANGTIDY+=	util.h

CPPCHECK+=	clang.c
CPPCHECK+=	comment.c
CPPCHECK+=	cpp-align.c
CPPCHECK+=	cpp-include.c
CPPCHECK+=	diff.c
CPPCHECK+=	doc.c
CPPCHECK+=	error.c
CPPCHECK+=	expr.c
CPPCHECK+=	file.c
CPPCHECK+=	fs.c
CPPCHECK+=	fuzz-style.c
CPPCHECK+=	knfmt.c
CPPCHECK+=	lexer.c
CPPCHECK+=	options.c
CPPCHECK+=	parser-attributes.c
CPPCHECK+=	parser-braces.c
CPPCHECK+=	parser-cpp.c
CPPCHECK+=	parser-decl.c
CPPCHECK+=	parser-expr.c
CPPCHECK+=	parser-extern.c
CPPCHECK+=	parser-func.c
CPPCHECK+=	parser-stmt-asm.c
CPPCHECK+=	parser-stmt-expr.c
CPPCHECK+=	parser-stmt.c
CPPCHECK+=	parser-type.c
CPPCHECK+=	parser.c
CPPCHECK+=	ruler.c
CPPCHECK+=	simple-decl-forward.c
CPPCHECK+=	simple-decl-proto.c
CPPCHECK+=	simple-decl.c
CPPCHECK+=	simple-static.c
CPPCHECK+=	simple-stmt.c
CPPCHECK+=	simple.c
CPPCHECK+=	style.c
CPPCHECK+=	t.c
CPPCHECK+=	token.c
CPPCHECK+=	trace.c
CPPCHECK+=	util.c

CPPCHECKFLAGS+=	--quiet
CPPCHECKFLAGS+=	--check-level=exhaustive
CPPCHECKFLAGS+=	--enable=all
CPPCHECKFLAGS+=	--error-exitcode=1
CPPCHECKFLAGS+=	--max-configs=2
CPPCHECKFLAGS+=	--suppress-xml=cppcheck-suppressions.xml
CPPCHECKFLAGS+=	${CPPFLAGS}

SHLINT+=	configure
SHLINT+=	tests/cp.sh
SHLINT+=	tests/diff.sh
SHLINT+=	tests/enoent.sh
SHLINT+=	tests/fd.sh
SHLINT+=	tests/git.sh
SHLINT+=	tests/include-categories.sh
SHLINT+=	tests/knfmt.sh
SHLINT+=	tests/simple.sh
SHLINT+=	tests/stdin.sh

SHELLCHECKFLAGS+=	-f gcc
SHELLCHECKFLAGS+=	-s ksh
SHELLCHECKFLAGS+=	-o add-default-case
SHELLCHECKFLAGS+=	-o avoid-nullary-conditions
SHELLCHECKFLAGS+=	-o quote-safe-variables

all: ${PROG_knfmt}

${PROG_knfmt}: ${OBJS_knfmt}
	${CC} ${DEBUG} ${NO_SANITIZE_FUZZER} -o ${PROG_knfmt} ${OBJS_knfmt} ${LDFLAGS}

${PROG_test}: ${OBJS_test}
	${CC} ${DEBUG} ${NO_SANITIZE_FUZZER} -o ${PROG_test} ${OBJS_test} ${LDFLAGS}

clean:
	rm -f ${DEPS_knfmt} ${OBJS_knfmt} ${PROG_knfmt} \
		${DEPS_test} ${OBJS_test} ${PROG_test} \
		${DEPS_fuzz-style} ${OBJS_fuzz-style} ${PROG_fuzz-style}
.PHONY: clean

cleandir: clean
	cd ${.CURDIR} && rm -f config.h config.log config.mk
.PHONY: cleandir

dist:
	set -e; p=knfmt-${VERSION}; cd ${.CURDIR}; \
	git archive --output $$p.tar.gz --prefix $$p/ v${VERSION}; \
	sha256 $$p.tar.gz >$$p.sha256
.PHONY: dist

format: ${PROG_knfmt}
	cd ${.CURDIR} && ${.OBJDIR}/${PROG_knfmt} -is ${KNFMT}
.PHONY: format

fuzz: ${PROG_fuzz-style}

${PROG_fuzz-style}: ${OBJS_fuzz-style}
	${CC} ${DEBUG} -o ${PROG_fuzz-style} ${OBJS_fuzz-style} ${LDFLAGS}

install: all
	@mkdir -p ${DESTDIR}${BINDIR}
	${INSTALL} ${PROG_knfmt} ${DESTDIR}${BINDIR}
	@mkdir -p ${DESTDIR}${MANDIR}/man1
	${INSTALL_MAN} ${.CURDIR}/knfmt.1 ${DESTDIR}${MANDIR}/man1
.PHONY: install

lint: ${PROG_knfmt}
	cd ${.CURDIR} && ${.OBJDIR}/${PROG_knfmt} -ds ${KNFMT}
	cd ${.CURDIR} && mandoc -Tlint -Wstyle knfmt.1
.PHONY: lint

lint-clang-tidy:
	cd ${.CURDIR} && echo ${CLANGTIDY} | xargs printf '%s\n' | \
		xargs -I{} clang-tidy --quiet {} -- ${CPPFLAGS}
.PHONY: lint-clang-tidy

lint-cppcheck:
	cd ${.CURDIR} && cppcheck ${CPPCHECKFLAGS} ${CPPCHECK}
.PHONY: lint-cppcheck

IWYU?=	include-what-you-use
lint-include-what-you-use:
	cd ${.CURDIR} && echo ${CPPCHECK} | xargs printf '%s\n' | \
		xargs -I{} ${IWYU} ${CPPFLAGS} {}
.PHONY: lint-include-what-you-use

lint-shellcheck:
	cd ${.CURDIR} && shellcheck ${SHELLCHECKFLAGS} ${SHLINT}
.PHONY: lint-shellcheck

test: ${PROG_knfmt} test-${PROG_test}
	${MAKE} -C ${.CURDIR}/tests "KNFMT=${.OBJDIR}/${PROG_knfmt}"
.PHONY: test

test-${PROG_test}: ${PROG_test}
	${EXEC} ./${PROG_test}

-include ${DEPS_knfmt}
-include ${DEPS_test}
-include ${DEPS_fuzz-style}

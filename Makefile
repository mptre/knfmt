include ${.CURDIR}/config.mk

VERSION=	5.0.0rc5

SRCS+=	arenas.c
SRCS+=	clang.c
SRCS+=	comment.c
SRCS+=	compat-pledge.c
SRCS+=	cpp-format.c
SRCS+=	cpp-include-guard.c
SRCS+=	cpp-include.c
SRCS+=	diff.c
SRCS+=	doc.c
SRCS+=	error.c
SRCS+=	expr.c
SRCS+=	file.c
SRCS+=	lexer.c
SRCS+=	libks/arena-buffer.c
SRCS+=	libks/arena-vector.c
SRCS+=	libks/arena.c
SRCS+=	libks/arithmetic.c
SRCS+=	libks/buffer.c
SRCS+=	libks/capabilities-x86.c
SRCS+=	libks/consistency.c
SRCS+=	libks/exec.c
SRCS+=	libks/fs.c
SRCS+=	libks/init-x86_64.c
SRCS+=	libks/init.c
SRCS+=	libks/map.c
SRCS+=	libks/string-x86_64.S
SRCS+=	libks/string.c
SRCS+=	libks/vector.c
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
SRCS+=	path.c
SRCS+=	ruler.c
SRCS+=	simple-attributes.c
SRCS+=	simple-decl-forward.c
SRCS+=	simple-decl-proto.c
SRCS+=	simple-decl.c
SRCS+=	simple-expr-printf.c
SRCS+=	simple-implicit-int.c
SRCS+=	simple-static.c
SRCS+=	simple-stmt-empty-loop.c
SRCS+=	simple-stmt-switch.c
SRCS+=	simple-stmt.c
SRCS+=	simple.c
SRCS+=	style.c
SRCS+=	token.c
SRCS+=	trace.c
SRCS+=	util.c

SRCS_knfmt+=	${SRCS}
SRCS_knfmt+=	knfmt.c
OBJS_knfmt:=	${SRCS_knfmt:.c=.o}
OBJS_knfmt:=	${OBJS_knfmt:.S=.o}
DEPS_knfmt=	${OBJS_knfmt:.o=.d}
PROG_knfmt=	knfmt

SRCS_test+=	${SRCS}
SRCS_test+=	t.c
OBJS_test:=	${SRCS_test:.c=.o}
OBJS_test:=	${OBJS_test:.S=.o}
DEPS_test=	${OBJS_test:.o=.d}
PROG_test=	t

SRCS_fuzz-dict+=	${SRCS}
SRCS_fuzz-dict+=	fuzz-dict.c
OBJS_fuzz-dict:=	${SRCS_fuzz-dict:.c=.o}
OBJS_fuzz-dict:=	${OBJS_fuzz-dict:.S=.o}
DEPS_fuzz-dict=		${OBJS_fuzz-dict:.o=.d}
PROG_fuzz-dict=		fuzz-dict

SRCS_fuzz-style+=	${SRCS}
SRCS_fuzz-style+=	fuzz-style.c
OBJS_fuzz-style:=	${SRCS_fuzz-style:.c=.o}
OBJS_fuzz-style:=	${OBJS_fuzz-style:.S=.o}
DEPS_fuzz-style=	${OBJS_fuzz-style:.o=.d}
PROG_fuzz-style=	fuzz-style
DICT_fuzz-style=	style.dict

SRCS_benchmark+=	${SRCS}
SRCS_benchmark+=	benchmark.cpp
OBJS_benchmark:=	${SRCS_benchmark}
OBJS_benchmark:=	${OBJS_benchmark:.c=.o}
OBJS_benchmark:=	${OBJS_benchmark:.cpp=.o}
OBJS_benchmark:=	${OBJS_benchmark:.S=.o}
DEPS_benchmark=		${OBJS_benchmark:.o=.d}
PROG_benchmark=		benchmark

KNFMT+=	arenas.c
KNFMT+=	arenas.h
KNFMT+=	clang.c
KNFMT+=	clang.h
KNFMT+=	comment.c
KNFMT+=	comment.h
KNFMT+=	compat-pledge.c
KNFMT+=	cpp-format.c
KNFMT+=	cpp-format.h
KNFMT+=	cpp-include-guard.c
KNFMT+=	cpp-include-guard.h
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
KNFMT+=	fuzz-dict.c
KNFMT+=	fuzz-style.c
KNFMT+=	knfmt.c
KNFMT+=	lexer-callbacks.h
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
KNFMT+=	path.c
KNFMT+=	path.h
KNFMT+=	ruler.c
KNFMT+=	ruler.h
KNFMT+=	simple-attributes.c
KNFMT+=	simple-attributes.h
KNFMT+=	simple-decl-forward.c
KNFMT+=	simple-decl-forward.h
KNFMT+=	simple-decl-proto.c
KNFMT+=	simple-decl-proto.h
KNFMT+=	simple-decl.c
KNFMT+=	simple-decl.h
KNFMT+=	simple-expr-printf.c
KNFMT+=	simple-expr-printf.h
KNFMT+=	simple-implicit-int.c
KNFMT+=	simple-implicit-int.h
KNFMT+=	simple-static.c
KNFMT+=	simple-static.h
KNFMT+=	simple-stmt-empty-loop.c
KNFMT+=	simple-stmt-empty-loop.h
KNFMT+=	simple-stmt-switch.c
KNFMT+=	simple-stmt-switch.h
KNFMT+=	simple-stmt.c
KNFMT+=	simple-stmt.h
KNFMT+=	simple.c
KNFMT+=	simple.h
KNFMT+=	style.c
KNFMT+=	style.h
KNFMT+=	t.c
KNFMT+=	token.c
KNFMT+=	token.h
KNFMT+=	trace-types.h
KNFMT+=	trace.c
KNFMT+=	trace.h
KNFMT+=	util.c
KNFMT+=	util.h

CLANGTIDY+=	arenas.c
CLANGTIDY+=	arenas.h
CLANGTIDY+=	clang.c
CLANGTIDY+=	clang.h
CLANGTIDY+=	comment.c
CLANGTIDY+=	comment.h
CLANGTIDY+=	cpp-format.c
CLANGTIDY+=	cpp-format.h
CLANGTIDY+=	cpp-include-guard.c
CLANGTIDY+=	cpp-include-guard.h
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
CLANGTIDY+=	fuzz-dict.c
CLANGTIDY+=	fuzz-style.c
CLANGTIDY+=	knfmt.c
CLANGTIDY+=	lexer-callbacks.h
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
CLANGTIDY+=	path.c
CLANGTIDY+=	path.h
CLANGTIDY+=	ruler.c
CLANGTIDY+=	ruler.h
CLANGTIDY+=	simple-attributes.c
CLANGTIDY+=	simple-attributes.h
CLANGTIDY+=	simple-decl-forward.c
CLANGTIDY+=	simple-decl-forward.h
CLANGTIDY+=	simple-decl-proto.c
CLANGTIDY+=	simple-decl-proto.h
CLANGTIDY+=	simple-decl.c
CLANGTIDY+=	simple-decl.h
CLANGTIDY+=	simple-expr-printf.c
CLANGTIDY+=	simple-expr-printf.h
CLANGTIDY+=	simple-implicit-int.c
CLANGTIDY+=	simple-implicit-int.h
CLANGTIDY+=	simple-static.c
CLANGTIDY+=	simple-static.h
CLANGTIDY+=	simple-stmt-empty-loop.c
CLANGTIDY+=	simple-stmt-empty-loop.h
CLANGTIDY+=	simple-stmt-switch.c
CLANGTIDY+=	simple-stmt-switch.h
CLANGTIDY+=	simple-stmt.c
CLANGTIDY+=	simple-stmt.h
CLANGTIDY+=	simple.c
CLANGTIDY+=	simple.h
CLANGTIDY+=	style.c
CLANGTIDY+=	style.h
CLANGTIDY+=	t.c
CLANGTIDY+=	token.c
CLANGTIDY+=	token.h
CLANGTIDY+=	trace-types.h
CLANGTIDY+=	trace.c
CLANGTIDY+=	trace.h
CLANGTIDY+=	util.c
CLANGTIDY+=	util.h

CPPCHECK+=	arenas.c
CPPCHECK+=	clang.c
CPPCHECK+=	comment.c
CPPCHECK+=	cpp-format.c
CPPCHECK+=	cpp-include-guard.c
CPPCHECK+=	cpp-include.c
CPPCHECK+=	diff.c
CPPCHECK+=	doc.c
CPPCHECK+=	error.c
CPPCHECK+=	expr.c
CPPCHECK+=	file.c
CPPCHECK+=	fuzz-dict.c
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
CPPCHECK+=	path.c
CPPCHECK+=	ruler.c
CPPCHECK+=	simple-attributes.c
CPPCHECK+=	simple-decl-forward.c
CPPCHECK+=	simple-decl-proto.c
CPPCHECK+=	simple-decl.c
CPPCHECK+=	simple-expr-printf.c
CPPCHECK+=	simple-implicit-int.c
CPPCHECK+=	simple-static.c
CPPCHECK+=	simple-stmt-empty-loop.c
CPPCHECK+=	simple-stmt-switch.c
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

IWYU+=	arenas.c
IWYU+=	arenas.h
IWYU+=	clang.c
IWYU+=	clang.h
IWYU+=	comment.c
IWYU+=	comment.h
IWYU+=	cpp-format.c
IWYU+=	cpp-format.h
IWYU+=	cpp-include-guard.c
IWYU+=	cpp-include-guard.h
IWYU+=	cpp-include.c
IWYU+=	cpp-include.h
IWYU+=	diff.c
IWYU+=	diff.h
IWYU+=	doc.c
IWYU+=	doc.h
IWYU+=	error.c
IWYU+=	error.h
IWYU+=	expr.c
IWYU+=	expr.h
IWYU+=	file.c
IWYU+=	file.h
IWYU+=	fuzz-dict.c
IWYU+=	fuzz-style.c
IWYU+=	knfmt.c
IWYU+=	lexer-callbacks.h
IWYU+=	lexer.c
IWYU+=	lexer.h
IWYU+=	options.c
IWYU+=	options.h
IWYU+=	parser-attributes.c
IWYU+=	parser-attributes.h
IWYU+=	parser-braces.c
IWYU+=	parser-braces.h
IWYU+=	parser-cpp.c
IWYU+=	parser-cpp.h
IWYU+=	parser-decl.c
IWYU+=	parser-decl.h
IWYU+=	parser-expr.c
IWYU+=	parser-expr.h
IWYU+=	parser-extern.c
IWYU+=	parser-extern.h
IWYU+=	parser-func.c
IWYU+=	parser-func.h
IWYU+=	parser-priv.h
IWYU+=	parser-stmt-asm.c
IWYU+=	parser-stmt-asm.h
IWYU+=	parser-stmt-expr.c
IWYU+=	parser-stmt-expr.h
IWYU+=	parser-stmt.c
IWYU+=	parser-stmt.h
IWYU+=	parser-type.c
IWYU+=	parser-type.h
IWYU+=	parser.c
IWYU+=	parser.h
IWYU+=	path.c
IWYU+=	path.h
IWYU+=	ruler.c
IWYU+=	ruler.h
IWYU+=	simple-attributes.c
IWYU+=	simple-attributes.h
IWYU+=	simple-decl-forward.c
IWYU+=	simple-decl-forward.h
IWYU+=	simple-decl-proto.c
IWYU+=	simple-decl-proto.h
IWYU+=	simple-decl.c
IWYU+=	simple-decl.h
IWYU+=	simple-expr-printf.c
IWYU+=	simple-expr-printf.h
IWYU+=	simple-implicit-int.c
IWYU+=	simple-implicit-int.h
IWYU+=	simple-static.c
IWYU+=	simple-static.h
IWYU+=	simple-stmt-empty-loop.c
IWYU+=	simple-stmt-empty-loop.h
IWYU+=	simple-stmt-switch.c
IWYU+=	simple-stmt-switch.h
IWYU+=	simple-stmt.c
IWYU+=	simple-stmt.h
IWYU+=	simple.c
IWYU+=	simple.h
IWYU+=	style.c
IWYU+=	style.h
IWYU+=	t.c
IWYU+=	token.c
IWYU+=	token.h
IWYU+=	trace-types.h
IWYU+=	trace.c
IWYU+=	trace.h
IWYU+=	util.c
IWYU+=	util.h

IWYUFLAGS+=	-a arenas.h
IWYUFLAGS+=	-d config.h
IWYUFLAGS+=	-d clang.h:lexer-callbacks.h
IWYUFLAGS+=	-d file.c:diff.h
IWYUFLAGS+=	-d trace.h:options.h
IWYUFLAGS+=	${CPPFLAGS}

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
SHLINT+=	tests/style-enoent.sh

SHELLCHECKFLAGS+=	-f gcc
SHELLCHECKFLAGS+=	-s ksh
SHELLCHECKFLAGS+=	-o add-default-case
SHELLCHECKFLAGS+=	-o avoid-nullary-conditions
SHELLCHECKFLAGS+=	-o quote-safe-variables
SHELLCHECKFLAGS+=	-o require-variable-braces

all: ${PROG_knfmt}

${PROG_knfmt}: ${OBJS_knfmt}
	${CC} ${DEBUG} ${NO_SANITIZE_FUZZER} -o ${PROG_knfmt} ${OBJS_knfmt} ${LDFLAGS}

${PROG_test}: ${OBJS_test}
	${CC} ${DEBUG} ${NO_SANITIZE_FUZZER} -o ${PROG_test} ${OBJS_test} ${LDFLAGS}

${PROG_benchmark}: ${OBJS_benchmark}
	${CXX} ${DEBUG} -o ${PROG_benchmark} ${OBJS_benchmark} \
		${LDFLAGS_benchmark}

clean:
	rm -f ${DEPS_knfmt} ${OBJS_knfmt} ${PROG_knfmt} \
		${DEPS_test} ${OBJS_test} ${PROG_test} \
		${DEPS_fuzz-dict} ${OBJS_fuzz-dict} ${PROG_fuzz-dict} \
		${DEPS_fuzz-style} ${OBJS_fuzz-style} ${PROG_fuzz-style} ${DICT_fuzz-style} \
		${DEPS_benchmark} ${OBJS_benchmark} ${PROG_benchmark}
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

${PROG_fuzz-dict}: ${OBJS_fuzz-dict}
	${CC} ${DEBUG} ${NO_SANITIZE_FUZZER} -o ${PROG_fuzz-dict} ${OBJS_fuzz-dict} ${LDFLAGS}

${DICT_fuzz-style}: ${PROG_fuzz-dict}
	./${PROG_fuzz-dict} style >$@

${PROG_fuzz-style}: ${OBJS_fuzz-style} ${DICT_fuzz-style}
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
	cd ${.CURDIR} && clang-tidy --quiet ${CLANGTIDY} -- ${CPPFLAGS}
.PHONY: lint-clang-tidy

lint-cppcheck:
	cd ${.CURDIR} && cppcheck ${CPPCHECKFLAGS} ${CPPCHECK}
.PHONY: lint-cppcheck

lint-include-what-you-use:
	cd ${.CURDIR} && iwyu-filter ${IWYUFLAGS} -- ${IWYU}
.PHONY: lint-include-what-you-use

lint-shellcheck:
	cd ${.CURDIR} && shellcheck ${SHELLCHECKFLAGS} ${SHLINT}
.PHONY: lint-shellcheck

test: ${PROG_knfmt} test-${PROG_test}
	${MAKE} -C ${.CURDIR}/tests \
		"KNFMT=${.OBJDIR}/${PROG_knfmt}" \
		"VALGRINDRC=${.CURDIR}/.valgrindrc"
.PHONY: test

test-${PROG_test}: ${PROG_test}
	${EXEC} ./${PROG_test}

-include ${DEPS_knfmt}
-include ${DEPS_test}
-include ${DEPS_fuzz-dict}
-include ${DEPS_fuzz-style}
-include ${DEPS_benchmark}

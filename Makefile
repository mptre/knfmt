include ${.CURDIR}/config.mk

VERSION=	0.1.0

SRCS+=	buffer.c
SRCS+=	compat-errc.c
SRCS+=	compat-pledge.c
SRCS+=	compat-reallocarray.c
SRCS+=	compat-strtonum.c
SRCS+=	compat-vis.c
SRCS+=	compat-warnc.c
SRCS+=	doc.c
SRCS+=	expr.c
SRCS+=	lexer.c
SRCS+=	parser.c
SRCS+=	ruler.c
SRCS+=	util.c

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

DISTFILES+=	${SRCS_knfmt}
DISTFILES+=	${SRCS_test}
DISTFILES+=	GNUmakefile
DISTFILES+=	LICENSE
DISTFILES+=	Makefile
DISTFILES+=	compat-queue.h
DISTFILES+=	configure
DISTFILES+=	extern.h
DISTFILES+=	knfmt.1
DISTFILES+=	tests/GNUmakefile
DISTFILES+=	tests/Makefile
DISTFILES+=	tests/knfmt.sh
DISTFILES+=	token.h

# XXX tests/valid* tests/error*

all: ${PROG_knfmt}

${PROG_knfmt}: ${OBJS_knfmt}
	${CC} ${DEBUG} -o ${PROG_knfmt} ${OBJS_knfmt} ${LDFLAGS}

${PROG_test}: ${OBJS_test}
	${CC} ${DEBUG} -o ${PROG_test} ${OBJS_test} ${LDFLAGS}

clean:
	rm -f ${DEPS_knfmt} ${OBJS_knfmt} ${PROG_knfmt} \
		${DEPS_test} ${OBJS_test} ${PROG_test}
.PHONY: clean

dist:
	set -e; \
	d=knfmt-${VERSION}; \
	mkdir $$d; \
	for f in ${DISTFILES}; do \
		mkdir -p $$d/`dirname $$f`; \
		cp -p ${.CURDIR}/$$f $$d/$$f; \
	done; \
	find $$d -type d -exec touch -r ${.CURDIR}/Makefile {} \;; \
	tar czvf ${.CURDIR}/$$d.tar.gz $$d; \
	(cd ${.CURDIR}; sha256 $$d.tar.gz >$$d.sha256); \
	rm -r $$d
.PHONY: dist

distclean: clean
	rm -f ${.CURDIR}/config.h ${.CURDIR}/config.log ${.CURDIR}/config.mk \
		${.CURDIR}/knfmt-${VERSION}.tar.gz \
		${.CURDIR}/knfmt-${VERSION}.sha256
.PHONY: distclean

install: all
	@mkdir -p ${DESTDIR}${BINDIR}
	${INSTALL} ${PROG_knfmt} ${DESTDIR}${BINDIR}
	@mkdir -p ${DESTDIR}${MANDIR}/man1
	${INSTALL_MAN} ${.CURDIR}/knfmt.1 ${DESTDIR}${MANDIR}/man1
.PHONY: install

lint:
	mandoc -Tlint -Wstyle ${.CURDIR}/knfmt.1
.PHONY: lint

test: ${PROG_knfmt} test-${PROG_test}
	${MAKE} -C ${.CURDIR}/tests "KNFMT=${.OBJDIR}/${PROG_knfmt}"
.PHONY: test

test-${PROG_test}: ${PROG_test}
	./${PROG_test}

-include ${DEPS_knfmt}
-include ${DEPS_test}

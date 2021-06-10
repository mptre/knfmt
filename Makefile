include ${.CURDIR}/config.mk

VERSION=	0.1.0

SRCS+=	buffer.c
SRCS+=	compat-errc.c
SRCS+=	compat-pledge.c
SRCS+=	compat-reallocarray.c
SRCS+=	compat-warnc.c
SRCS+=	doc.c
SRCS+=	error.c
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
DISTFILES+=	CHANGELOG.md
DISTFILES+=	GNUmakefile
DISTFILES+=	LICENSE
DISTFILES+=	Makefile
DISTFILES+=	README.md
DISTFILES+=	compat-errc.c
DISTFILES+=	compat-pledge.c
DISTFILES+=	compat-queue.h
DISTFILES+=	compat-queue.h
DISTFILES+=	compat-reallocarray.c
DISTFILES+=	compat-uthash.h
DISTFILES+=	compat-warnc.c
DISTFILES+=	configure
DISTFILES+=	extern.h
DISTFILES+=	knfmt.1
DISTFILES+=	tests/GNUmakefile
DISTFILES+=	tests/Makefile
DISTFILES+=	tests/error-001.c
DISTFILES+=	tests/error-002.c
DISTFILES+=	tests/error-003.c
DISTFILES+=	tests/error-004.c
DISTFILES+=	tests/error-005.c
DISTFILES+=	tests/error-006.c
DISTFILES+=	tests/error-007.c
DISTFILES+=	tests/error-008.c
DISTFILES+=	tests/error-009.c
DISTFILES+=	tests/error-010.c
DISTFILES+=	tests/error-011.c
DISTFILES+=	tests/knfmt.sh
DISTFILES+=	tests/valid-001.c
DISTFILES+=	tests/valid-002.c
DISTFILES+=	tests/valid-003.c
DISTFILES+=	tests/valid-004.c
DISTFILES+=	tests/valid-005.c
DISTFILES+=	tests/valid-006.c
DISTFILES+=	tests/valid-007.c
DISTFILES+=	tests/valid-008.c
DISTFILES+=	tests/valid-009.c
DISTFILES+=	tests/valid-010.c
DISTFILES+=	tests/valid-011.c
DISTFILES+=	tests/valid-012.c
DISTFILES+=	tests/valid-013.c
DISTFILES+=	tests/valid-014.c
DISTFILES+=	tests/valid-015.c
DISTFILES+=	tests/valid-016.c
DISTFILES+=	tests/valid-017.c
DISTFILES+=	tests/valid-018.c
DISTFILES+=	tests/valid-019.c
DISTFILES+=	tests/valid-020.c
DISTFILES+=	tests/valid-021.c
DISTFILES+=	tests/valid-022.c
DISTFILES+=	tests/valid-023.c
DISTFILES+=	tests/valid-024.c
DISTFILES+=	tests/valid-025.c
DISTFILES+=	tests/valid-026.c
DISTFILES+=	tests/valid-027.c
DISTFILES+=	tests/valid-028.c
DISTFILES+=	tests/valid-029.c
DISTFILES+=	tests/valid-030.c
DISTFILES+=	tests/valid-031.c
DISTFILES+=	tests/valid-032.c
DISTFILES+=	tests/valid-033.c
DISTFILES+=	tests/valid-034.c
DISTFILES+=	tests/valid-035.c
DISTFILES+=	tests/valid-036.c
DISTFILES+=	tests/valid-037.c
DISTFILES+=	tests/valid-038.c
DISTFILES+=	tests/valid-039.c
DISTFILES+=	tests/valid-040.c
DISTFILES+=	tests/valid-041.c
DISTFILES+=	tests/valid-042.c
DISTFILES+=	tests/valid-043.c
DISTFILES+=	tests/valid-044.c
DISTFILES+=	tests/valid-045.c
DISTFILES+=	tests/valid-046.c
DISTFILES+=	tests/valid-047.c
DISTFILES+=	tests/valid-048.c
DISTFILES+=	tests/valid-049.c
DISTFILES+=	tests/valid-050.c
DISTFILES+=	tests/valid-051.c
DISTFILES+=	tests/valid-052.c
DISTFILES+=	tests/valid-053.c
DISTFILES+=	tests/valid-054.c
DISTFILES+=	tests/valid-055.c
DISTFILES+=	tests/valid-056.c
DISTFILES+=	tests/valid-057.c
DISTFILES+=	tests/valid-058.c
DISTFILES+=	tests/valid-059.c
DISTFILES+=	tests/valid-060.c
DISTFILES+=	tests/valid-061.c
DISTFILES+=	tests/valid-062.c
DISTFILES+=	tests/valid-063.c
DISTFILES+=	tests/valid-064.c
DISTFILES+=	tests/valid-065.c
DISTFILES+=	tests/valid-066.c
DISTFILES+=	tests/valid-067.c
DISTFILES+=	tests/valid-068.c
DISTFILES+=	tests/valid-069.c
DISTFILES+=	tests/valid-069.ok
DISTFILES+=	tests/valid-070.c
DISTFILES+=	tests/valid-070.ok
DISTFILES+=	tests/valid-071.c
DISTFILES+=	tests/valid-072.c
DISTFILES+=	tests/valid-073.c
DISTFILES+=	tests/valid-074.c
DISTFILES+=	tests/valid-075.c
DISTFILES+=	tests/valid-076.c
DISTFILES+=	tests/valid-077.c
DISTFILES+=	tests/valid-078.c
DISTFILES+=	tests/valid-079.c
DISTFILES+=	tests/valid-080.c
DISTFILES+=	tests/valid-080.ok
DISTFILES+=	tests/valid-081.c
DISTFILES+=	tests/valid-082.c
DISTFILES+=	tests/valid-083.c
DISTFILES+=	tests/valid-084.c
DISTFILES+=	tests/valid-085.c
DISTFILES+=	tests/valid-086.c
DISTFILES+=	tests/valid-087.c
DISTFILES+=	tests/valid-088.c
DISTFILES+=	tests/valid-089.c
DISTFILES+=	tests/valid-089.ok
DISTFILES+=	tests/valid-090.c
DISTFILES+=	tests/valid-091.ok
DISTFILES+=	tests/valid-091.c
DISTFILES+=	tests/valid-092.c
DISTFILES+=	tests/valid-093.c
DISTFILES+=	tests/valid-094.c
DISTFILES+=	tests/valid-095.c
DISTFILES+=	tests/valid-096.c
DISTFILES+=	tests/valid-097.c
DISTFILES+=	tests/valid-098.c
DISTFILES+=	tests/valid-099.c
DISTFILES+=	tests/valid-100.c
DISTFILES+=	tests/valid-101.c
DISTFILES+=	tests/valid-102.c
DISTFILES+=	tests/valid-103.c
DISTFILES+=	tests/valid-104.c
DISTFILES+=	tests/valid-105.c
DISTFILES+=	tests/valid-106.c
DISTFILES+=	tests/valid-107.c
DISTFILES+=	tests/valid-108.c
DISTFILES+=	tests/valid-109.c
DISTFILES+=	tests/valid-110.c
DISTFILES+=	tests/valid-111.c
DISTFILES+=	tests/valid-112.c
DISTFILES+=	tests/valid-113.c
DISTFILES+=	tests/valid-114.c
DISTFILES+=	tests/valid-115.c
DISTFILES+=	tests/valid-116.c
DISTFILES+=	tests/valid-117.c
DISTFILES+=	tests/valid-118.c
DISTFILES+=	tests/valid-119.c
DISTFILES+=	tests/valid-120.c
DISTFILES+=	tests/valid-121.c
DISTFILES+=	tests/valid-122.c
DISTFILES+=	tests/valid-122.ok
DISTFILES+=	tests/valid-123.c
DISTFILES+=	tests/valid-124.c
DISTFILES+=	tests/valid-125.c
DISTFILES+=	tests/valid-126.c
DISTFILES+=	tests/valid-127.c
DISTFILES+=	tests/valid-128.c
DISTFILES+=	tests/valid-129.c
DISTFILES+=	tests/valid-130.c
DISTFILES+=	tests/valid-131.c
DISTFILES+=	tests/valid-132.c
DISTFILES+=	tests/valid-133.c
DISTFILES+=	tests/valid-134.c
DISTFILES+=	tests/valid-135.c
DISTFILES+=	token.h

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
	${EXEC} ./${PROG_test}

-include ${DEPS_knfmt}
-include ${DEPS_test}

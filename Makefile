include ${.CURDIR}/config.mk

VERSION=	2.1.0

SRCS+=	buffer.c
SRCS+=	comment.c
SRCS+=	compat-errc.c
SRCS+=	compat-pledge.c
SRCS+=	compat-reallocarray.c
SRCS+=	compat-warnc.c
SRCS+=	cpp.c
SRCS+=	diff.c
SRCS+=	doc.c
SRCS+=	error.c
SRCS+=	expr.c
SRCS+=	lexer.c
SRCS+=	parser.c
SRCS+=	ruler.c
SRCS+=	simple-decl.c
SRCS+=	simple-stmt.c
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

KNFMT+=	buffer.c
KNFMT+=	comment.c
KNFMT+=	compat-pledge.c
KNFMT+=	cpp.c
KNFMT+=	diff.c
KNFMT+=	doc.c
KNFMT+=	error.c
KNFMT+=	expr.c
KNFMT+=	extern.h
KNFMT+=	knfmt.c
KNFMT+=	lexer.c
KNFMT+=	parser.c
KNFMT+=	ruler.c
KNFMT+=	simple-decl.c
KNFMT+=	simple-stmt.c
KNFMT+=	t.c
KNFMT+=	token.h
KNFMT+=	util.c

DISTFILES+=	CHANGELOG.md
DISTFILES+=	GNUmakefile
DISTFILES+=	LICENSE
DISTFILES+=	Makefile
DISTFILES+=	README.md
DISTFILES+=	buffer.c
DISTFILES+=	comment.c
DISTFILES+=	compat-errc.c
DISTFILES+=	compat-pledge.c
DISTFILES+=	compat-queue.h
DISTFILES+=	compat-reallocarray.c
DISTFILES+=	compat-uthash.h
DISTFILES+=	compat-warnc.c
DISTFILES+=	configure
DISTFILES+=	cpp.c
DISTFILES+=	diff.c
DISTFILES+=	doc.c
DISTFILES+=	error.c
DISTFILES+=	expr.c
DISTFILES+=	extern.h
DISTFILES+=	knfmt.1
DISTFILES+=	knfmt.c
DISTFILES+=	lexer.c
DISTFILES+=	parser.c
DISTFILES+=	ruler.c
DISTFILES+=	simple-decl.c
DISTFILES+=	simple-stmt.c
DISTFILES+=	t.c
DISTFILES+=	tests/GNUmakefile
DISTFILES+=	tests/Makefile
DISTFILES+=	tests/bug-002.c
DISTFILES+=	tests/bug-005.c
DISTFILES+=	tests/bug-006.c
DISTFILES+=	tests/diff-001.c
DISTFILES+=	tests/diff-001.ok
DISTFILES+=	tests/diff-001.patch
DISTFILES+=	tests/diff-002.c
DISTFILES+=	tests/diff-002.ok
DISTFILES+=	tests/diff-002.patch
DISTFILES+=	tests/diff-003.c
DISTFILES+=	tests/diff-003.ok
DISTFILES+=	tests/diff-003.patch
DISTFILES+=	tests/diff-004.c
DISTFILES+=	tests/diff-004.ok
DISTFILES+=	tests/diff-004.patch
DISTFILES+=	tests/diff-005.c
DISTFILES+=	tests/diff-005.ok
DISTFILES+=	tests/diff-005.patch
DISTFILES+=	tests/diff-006.c
DISTFILES+=	tests/diff-006.ok
DISTFILES+=	tests/diff-006.patch
DISTFILES+=	tests/diff-007.c
DISTFILES+=	tests/diff-007.ok
DISTFILES+=	tests/diff-007.patch
DISTFILES+=	tests/diff-008.c
DISTFILES+=	tests/diff-008.ok
DISTFILES+=	tests/diff-008.patch
DISTFILES+=	tests/diff-009.c
DISTFILES+=	tests/diff-009.ok
DISTFILES+=	tests/diff-009.patch
DISTFILES+=	tests/diff-010.c
DISTFILES+=	tests/diff-010.ok
DISTFILES+=	tests/diff-010.patch
DISTFILES+=	tests/diff-011.c
DISTFILES+=	tests/diff-011.ok
DISTFILES+=	tests/diff-011.patch
DISTFILES+=	tests/diff-012.c
DISTFILES+=	tests/diff-012.ok
DISTFILES+=	tests/diff-012.patch
DISTFILES+=	tests/diff-013.c
DISTFILES+=	tests/diff-013.ok
DISTFILES+=	tests/diff-013.patch
DISTFILES+=	tests/diff-014.c
DISTFILES+=	tests/diff-014.ok
DISTFILES+=	tests/diff-014.patch
DISTFILES+=	tests/diff-015.c
DISTFILES+=	tests/diff-015.ok
DISTFILES+=	tests/diff-015.patch
DISTFILES+=	tests/diff-016.c
DISTFILES+=	tests/diff-016.ok
DISTFILES+=	tests/diff-016.patch
DISTFILES+=	tests/diff-017.c
DISTFILES+=	tests/diff-017.ok
DISTFILES+=	tests/diff-017.patch
DISTFILES+=	tests/diff-018.c
DISTFILES+=	tests/diff-018.ok
DISTFILES+=	tests/diff-018.patch
DISTFILES+=	tests/diff-019.c
DISTFILES+=	tests/diff-019.ok
DISTFILES+=	tests/diff-019.patch
DISTFILES+=	tests/diff-020.c
DISTFILES+=	tests/diff-020.ok
DISTFILES+=	tests/diff-020.patch
DISTFILES+=	tests/diff-021.c
DISTFILES+=	tests/diff-021.ok
DISTFILES+=	tests/diff-021.patch
DISTFILES+=	tests/diff-022.c
DISTFILES+=	tests/diff-022.ok
DISTFILES+=	tests/diff-022.patch
DISTFILES+=	tests/diff-023.c
DISTFILES+=	tests/diff-023.ok
DISTFILES+=	tests/diff-023.patch
DISTFILES+=	tests/diff-024.c
DISTFILES+=	tests/diff-024.ok
DISTFILES+=	tests/diff-024.patch
DISTFILES+=	tests/diff-025.c
DISTFILES+=	tests/diff-025.ok
DISTFILES+=	tests/diff-025.patch
DISTFILES+=	tests/diff-026.c
DISTFILES+=	tests/diff-026.ok
DISTFILES+=	tests/diff-026.patch
DISTFILES+=	tests/diff-027.c
DISTFILES+=	tests/diff-027.ok
DISTFILES+=	tests/diff-027.patch
DISTFILES+=	tests/diff-028.c
DISTFILES+=	tests/diff-028.ok
DISTFILES+=	tests/diff-028.patch
DISTFILES+=	tests/diff-simple-001.c
DISTFILES+=	tests/diff-simple-001.ok
DISTFILES+=	tests/diff-simple-001.patch
DISTFILES+=	tests/diff-simple-002.c
DISTFILES+=	tests/diff-simple-002.ok
DISTFILES+=	tests/diff-simple-002.patch
DISTFILES+=	tests/error-001.c
DISTFILES+=	tests/error-002.c
DISTFILES+=	tests/error-003.c
DISTFILES+=	tests/error-004.c
DISTFILES+=	tests/error-005.c
DISTFILES+=	tests/error-007.c
DISTFILES+=	tests/error-008.c
DISTFILES+=	tests/error-009.c
DISTFILES+=	tests/error-010.c
DISTFILES+=	tests/error-013.c
DISTFILES+=	tests/error-014.c
DISTFILES+=	tests/error-015.c
DISTFILES+=	tests/error-016.c
DISTFILES+=	tests/error-017.c
DISTFILES+=	tests/error-018.c
DISTFILES+=	tests/error-019.c
DISTFILES+=	tests/error-020.c
DISTFILES+=	tests/error-021.c
DISTFILES+=	tests/error-022.c
DISTFILES+=	tests/error-023.c
DISTFILES+=	tests/error-024.c
DISTFILES+=	tests/error-025.c
DISTFILES+=	tests/error-026.c
DISTFILES+=	tests/error-027.c
DISTFILES+=	tests/error-028.c
DISTFILES+=	tests/error-029.c
DISTFILES+=	tests/error-030.c
DISTFILES+=	tests/error-031.c
DISTFILES+=	tests/error-032.c
DISTFILES+=	tests/error-033.c
DISTFILES+=	tests/error-034.c
DISTFILES+=	tests/error-035.c
DISTFILES+=	tests/error-036.c
DISTFILES+=	tests/error-037.c
DISTFILES+=	tests/error-038.c
DISTFILES+=	tests/error-039.c
DISTFILES+=	tests/knfmt.sh
DISTFILES+=	tests/simple-001.c
DISTFILES+=	tests/simple-001.ok
DISTFILES+=	tests/simple-002.c
DISTFILES+=	tests/simple-002.ok
DISTFILES+=	tests/simple-003.c
DISTFILES+=	tests/simple-003.ok
DISTFILES+=	tests/simple-004.c
DISTFILES+=	tests/simple-004.ok
DISTFILES+=	tests/simple-005.c
DISTFILES+=	tests/simple-005.ok
DISTFILES+=	tests/simple-006.c
DISTFILES+=	tests/simple-006.ok
DISTFILES+=	tests/simple-007.c
DISTFILES+=	tests/simple-007.ok
DISTFILES+=	tests/simple-008.c
DISTFILES+=	tests/simple-008.ok
DISTFILES+=	tests/simple-009.c
DISTFILES+=	tests/simple-009.ok
DISTFILES+=	tests/simple-010.c
DISTFILES+=	tests/simple-010.ok
DISTFILES+=	tests/simple-011.c
DISTFILES+=	tests/simple-012.c
DISTFILES+=	tests/simple-012.ok
DISTFILES+=	tests/simple-013.c
DISTFILES+=	tests/simple-013.ok
DISTFILES+=	tests/simple-014.c
DISTFILES+=	tests/simple-014.ok
DISTFILES+=	tests/simple-015.c
DISTFILES+=	tests/simple-015.ok
DISTFILES+=	tests/trace-001.c
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
DISTFILES+=	tests/valid-039.ok
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
DISTFILES+=	tests/valid-091.c
DISTFILES+=	tests/valid-091.ok
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
DISTFILES+=	tests/valid-136.c
DISTFILES+=	tests/valid-136.ok
DISTFILES+=	tests/valid-137.c
DISTFILES+=	tests/valid-137.ok
DISTFILES+=	tests/valid-138.c
DISTFILES+=	tests/valid-139.c
DISTFILES+=	tests/valid-140.c
DISTFILES+=	tests/valid-141.c
DISTFILES+=	tests/valid-142.c
DISTFILES+=	tests/valid-142.ok
DISTFILES+=	tests/valid-143.c
DISTFILES+=	tests/valid-144.c
DISTFILES+=	tests/valid-145.c
DISTFILES+=	tests/valid-145.ok
DISTFILES+=	tests/valid-146.c
DISTFILES+=	tests/valid-147.c
DISTFILES+=	tests/valid-148.c
DISTFILES+=	tests/valid-149.c
DISTFILES+=	tests/valid-150.c
DISTFILES+=	tests/valid-150.ok
DISTFILES+=	tests/valid-151.c
DISTFILES+=	tests/valid-152.c
DISTFILES+=	tests/valid-153.c
DISTFILES+=	tests/valid-153.ok
DISTFILES+=	tests/valid-154.c
DISTFILES+=	tests/valid-155.c
DISTFILES+=	tests/valid-155.ok
DISTFILES+=	tests/valid-156.c
DISTFILES+=	tests/valid-156.ok
DISTFILES+=	tests/valid-157.c
DISTFILES+=	tests/valid-157.ok
DISTFILES+=	tests/valid-158.c
DISTFILES+=	tests/valid-158.ok
DISTFILES+=	tests/valid-159.c
DISTFILES+=	tests/valid-160.c
DISTFILES+=	tests/valid-161.c
DISTFILES+=	tests/valid-162.c
DISTFILES+=	tests/valid-163.c
DISTFILES+=	tests/valid-164.c
DISTFILES+=	tests/valid-165.c
DISTFILES+=	tests/valid-166.c
DISTFILES+=	tests/valid-167.c
DISTFILES+=	tests/valid-168.c
DISTFILES+=	tests/valid-169.c
DISTFILES+=	tests/valid-170.c
DISTFILES+=	tests/valid-170.ok
DISTFILES+=	tests/valid-171.c
DISTFILES+=	tests/valid-172.c
DISTFILES+=	tests/valid-173.c
DISTFILES+=	tests/valid-174.c
DISTFILES+=	tests/valid-175.c
DISTFILES+=	tests/valid-176.c
DISTFILES+=	tests/valid-177.c
DISTFILES+=	tests/valid-178.c
DISTFILES+=	tests/valid-179.c
DISTFILES+=	tests/valid-180.c
DISTFILES+=	tests/valid-181.c
DISTFILES+=	tests/valid-181.ok
DISTFILES+=	tests/valid-182.c
DISTFILES+=	tests/valid-183.c
DISTFILES+=	tests/valid-184.c
DISTFILES+=	tests/valid-185.c
DISTFILES+=	tests/valid-186.c
DISTFILES+=	tests/valid-187.c
DISTFILES+=	tests/valid-187.ok
DISTFILES+=	tests/valid-188.c
DISTFILES+=	tests/valid-189.c
DISTFILES+=	tests/valid-189.ok
DISTFILES+=	tests/valid-190.c
DISTFILES+=	tests/valid-190.ok
DISTFILES+=	tests/valid-191.c
DISTFILES+=	tests/valid-191.ok
DISTFILES+=	tests/valid-192.c
DISTFILES+=	tests/valid-193.c
DISTFILES+=	tests/valid-194.c
DISTFILES+=	tests/valid-194.ok
DISTFILES+=	tests/valid-195.c
DISTFILES+=	tests/valid-195.ok
DISTFILES+=	tests/valid-196.c
DISTFILES+=	tests/valid-196.ok
DISTFILES+=	tests/valid-197.c
DISTFILES+=	tests/valid-198.c
DISTFILES+=	tests/valid-198.ok
DISTFILES+=	tests/valid-199.c
DISTFILES+=	tests/valid-199.ok
DISTFILES+=	tests/valid-200.c
DISTFILES+=	tests/valid-201.c
DISTFILES+=	tests/valid-202.c
DISTFILES+=	tests/valid-203.c
DISTFILES+=	tests/valid-204.c
DISTFILES+=	tests/valid-205.c
DISTFILES+=	tests/valid-206.c
DISTFILES+=	tests/valid-207.c
DISTFILES+=	tests/valid-208.c
DISTFILES+=	tests/valid-208.ok
DISTFILES+=	tests/valid-209.c
DISTFILES+=	tests/valid-209.ok
DISTFILES+=	tests/valid-210.c
DISTFILES+=	tests/valid-210.ok
DISTFILES+=	tests/valid-211.c
DISTFILES+=	tests/valid-212.c
DISTFILES+=	tests/valid-212.ok
DISTFILES+=	tests/valid-213.c
DISTFILES+=	tests/valid-213.ok
DISTFILES+=	tests/valid-214.c
DISTFILES+=	tests/valid-215.c
DISTFILES+=	tests/valid-216.c
DISTFILES+=	tests/valid-217.c
DISTFILES+=	tests/valid-218.c
DISTFILES+=	tests/valid-219.c
DISTFILES+=	tests/valid-220.c
DISTFILES+=	tests/valid-221.c
DISTFILES+=	tests/valid-223.c
DISTFILES+=	tests/valid-224.c
DISTFILES+=	tests/valid-224.ok
DISTFILES+=	tests/valid-225.c
DISTFILES+=	tests/valid-225.ok
DISTFILES+=	tests/valid-226.c
DISTFILES+=	tests/valid-227.c
DISTFILES+=	tests/valid-229.c
DISTFILES+=	tests/valid-230.c
DISTFILES+=	tests/valid-231.c
DISTFILES+=	tests/valid-231.ok
DISTFILES+=	tests/valid-232.c
DISTFILES+=	tests/valid-232.ok
DISTFILES+=	tests/valid-233.c
DISTFILES+=	tests/valid-233.ok
DISTFILES+=	tests/valid-234.c
DISTFILES+=	tests/valid-234.ok
DISTFILES+=	tests/valid-235.c
DISTFILES+=	tests/valid-235.ok
DISTFILES+=	tests/valid-236.c
DISTFILES+=	tests/valid-236.ok
DISTFILES+=	tests/valid-237.c
DISTFILES+=	tests/valid-238.c
DISTFILES+=	tests/valid-239.c
DISTFILES+=	tests/valid-239.ok
DISTFILES+=	tests/valid-240.c
DISTFILES+=	tests/valid-241.c
DISTFILES+=	tests/valid-242.c
DISTFILES+=	tests/valid-243.c
DISTFILES+=	tests/valid-243.ok
DISTFILES+=	token.h
DISTFILES+=	util.c

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

format: ${PROG_knfmt}
	cd ${.CURDIR} && ${.OBJDIR}/${PROG_knfmt} -is ${KNFMT}
.PHONY: format

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

test: ${PROG_knfmt} test-${PROG_test}
	${MAKE} -C ${.CURDIR}/tests "KNFMT=${.OBJDIR}/${PROG_knfmt}"
.PHONY: test

test-${PROG_test}: ${PROG_test}
	${EXEC} ./${PROG_test}

-include ${DEPS_knfmt}
-include ${DEPS_test}

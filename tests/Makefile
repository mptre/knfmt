TESTS+=	error-001.c
TESTS+=	error-002.c
TESTS+=	error-003.c
TESTS+=	error-004.c
TESTS+=	error-005.c
TESTS+=	error-006.c
TESTS+=	error-007.c
TESTS+=	error-008.c

TESTS+=	valid-001.c
TESTS+=	valid-002.c
TESTS+=	valid-003.c
TESTS+=	valid-004.c
TESTS+=	valid-005.c
TESTS+=	valid-006.c
TESTS+=	valid-007.c
TESTS+=	valid-008.c
TESTS+=	valid-009.c
TESTS+=	valid-010.c
TESTS+=	valid-011.c
TESTS+=	valid-012.c
TESTS+=	valid-013.c
TESTS+=	valid-014.c
TESTS+=	valid-015.c
TESTS+=	valid-016.c
TESTS+=	valid-017.c
TESTS+=	valid-018.c
TESTS+=	valid-019.c
TESTS+=	valid-020.c
TESTS+=	valid-021.c
TESTS+=	valid-022.c
TESTS+=	valid-023.c
TESTS+=	valid-024.c
TESTS+=	valid-025.c
TESTS+=	valid-026.c
TESTS+=	valid-027.c
TESTS+=	valid-028.c
TESTS+=	valid-029.c
TESTS+=	valid-030.c
TESTS+=	valid-031.c
TESTS+=	valid-032.c
TESTS+=	valid-033.c
TESTS+=	valid-034.c
TESTS+=	valid-035.c
TESTS+=	valid-036.c
TESTS+=	valid-037.c
TESTS+=	valid-038.c
TESTS+=	valid-039.c
TESTS+=	valid-040.c
TESTS+=	valid-041.c
TESTS+=	valid-042.c
TESTS+=	valid-043.c
TESTS+=	valid-044.c
TESTS+=	valid-045.c
TESTS+=	valid-046.c
TESTS+=	valid-047.c
TESTS+=	valid-048.c
TESTS+=	valid-049.c
TESTS+=	valid-050.c
TESTS+=	valid-051.c
TESTS+=	valid-052.c
TESTS+=	valid-053.c
TESTS+=	valid-054.c
TESTS+=	valid-055.c
TESTS+=	valid-056.c
TESTS+=	valid-057.c
TESTS+=	valid-058.c
TESTS+=	valid-059.c
TESTS+=	valid-060.c
TESTS+=	valid-061.c
TESTS+=	valid-062.c
TESTS+=	valid-063.c
TESTS+=	valid-064.c
TESTS+=	valid-065.c
TESTS+=	valid-066.c
TESTS+=	valid-067.c
TESTS+=	valid-068.c
TESTS+=	valid-069.c
TESTS+=	valid-070.c
TESTS+=	valid-071.c
TESTS+=	valid-072.c
TESTS+=	valid-073.c
TESTS+=	valid-074.c
TESTS+=	valid-075.c

TESTS+=	../buffer.c
TESTS+=	../compat-errc.c
TESTS+=	../compat-pledge.c
TESTS+=	../compat-queue.h
TESTS+=	../compat-strtonum.c
TESTS+=	../compat-vis.c
TESTS+=	../compat-warnc.c
TESTS+=	../doc.c
TESTS+=	../expr.c
TESTS+=	../extern.h
TESTS+=	../knfmt.c
TESTS+=	../lexer.c
TESTS+=	../parser.c
TESTS+=	../ruler.c
TESTS+=	../t.c
TESTS+=	../token.h
TESTS+=	../util.c

.SUFFIXES: .c .h .fake

.c.fake:
	sh ${.CURDIR}/knfmt.sh $<

.h.fake:
	sh ${.CURDIR}/knfmt.sh $<

all: ${TESTS:.c=.fake}
all: ${TESTS:.h=.fake}

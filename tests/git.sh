# Ensure diff mode works when invoking knfmt from a nested Git repository
# directory.

set -e

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap 'rm -r $_wrkdir' EXIT
cd "$_wrkdir"

mkdir .git

mkdir src
cd src
printf 'int x;\n' >test.c.orig
printf 'int x;\nint y;\n' >test.c
diff -u -L a/src/test.c -L b/src/test.c test.c.orig test.c |
${EXEC:-} "$KNFMT" -D >/dev/null

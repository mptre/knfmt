# File descriptor leak during inplace edit.

set -e

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap 'rm -r $_wrkdir' EXIT
cd "$_wrkdir"

_n=10

_i=0
while [ "$_i" -lt "$_n" ]; do
	printf 'int\nx;\n' >"${_i}.c"
	_i="$((_i + 1))"
done

# Intentionally not using EXEC as valgrind requires more file descriptors.
(ulimit -n "$((_n + 3))"; "$KNFMT" -i ./*.c)

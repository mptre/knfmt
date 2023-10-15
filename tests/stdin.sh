# Inplace edit of stdin does not make sense.

set -e

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap 'rm -r $_wrkdir' EXIT
cd "$_wrkdir"

printf 'int\na;\n' | ${EXEC:-} "$KNFMT" -i >"${_wrkdir}/out" 2>&1 && exit 1
if ! grep -q 'usage' "${_wrkdir}/out"; then
	cat "${_wrkdir}/out"
	exit 1
fi

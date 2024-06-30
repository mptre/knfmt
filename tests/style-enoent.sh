# No such file or directory.

set -e

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap 'rm -r $_wrkdir' EXIT
_out="${_wrkdir}/out"

(cd "${_wrkdir}" && ${EXEC:-} "${KNFMT}" -c enoent) >"${_out}" 2>&1 && exit 1
diff -u "${_out}" - <<EOF
knfmt: enoent: No such file or directory
EOF

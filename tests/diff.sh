# Exercise diff mode.

set -e

[ -z "${VALGRINDRC:-}" ] || export "VALGRIND_OPTS=$(xargs <"${VALGRINDRC}")"

${EXEC:-} "${KNFMT}" -d <<EOF 2>&1 >/dev/null || exit 0
int main(void) { return 0; }
EOF

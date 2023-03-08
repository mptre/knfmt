# Exercise diff mode.

set -e

${EXEC:-} "$KNFMT" -d <<EOF 2>&1 >/dev/null || exit 0
int main(void) { return 0; }
EOF

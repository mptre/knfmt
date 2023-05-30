# Simple mode not reset regression.

set -e

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap 'rm -r $_wrkdir' EXIT
cd "$_wrkdir"

cat <<'EOF' >a.c
int
main(int argc, char *argv[])
{
	while ((0))
		continue;
	return 0;
}
EOF

cat <<'EOF' >b.c
int
main(void)
{
	if (0)
		return 0;
	else {
		int x = 0;

		return x;
	}
}
EOF

! ${EXEC:-} "$KNFMT" -ds a.c b.c >/dev/null

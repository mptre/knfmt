#/bin/sh

set -e

# commstrip file
#
# Removed the leading block comment from file.
commstrip() {
	awk '!p && !length($0) {p=1; next} p {print}' "$1"
}

case "$1" in
error-*)
	_err=0
	${KNFMT} "$1" >/dev/null 2>&1 || _err="$?"
	if [ "$_err" -ne 1 ]; then
		echo "${1}; expected exit 1, got ${_err}" 1>&2
		exit 1
	fi
	;;
*)
	_ok="${1%.c}.ok"
	if [ -e "$_ok" ]; then
		_err=0
		_tmp="$(mktemp -t knfmt.XXXXXX)"
		commstrip "$1" >"$_tmp"
		"${KNFMT}" -v "$_tmp" | diff -u -L "$1" -L "$_ok" "$_ok" - || _err="$?"
		rm -f "$_tmp"
		exit "$_err"
	else
		"${KNFMT}" -dv "$1"
	fi
	;;
esac

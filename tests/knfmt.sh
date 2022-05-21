#/bin/sh

set -e

# commstrip file
#
# Removed the leading block comment from file.
commstrip() {
	awk '!p && !length($0) {p=1; next} p {print}' "$1"
}

# hascomm file
#
# Returns zero if the given file has a comment.
hascomm() {
	local _f

	_f="$1"; : "${_f:?}"

	if ! head -1 "$_f" | grep -q '^/\*$'; then
		echo "${_f}: missing test case comment" 1>&2
		return 1
	fi
}

# testcase [-b] [-q] file [-- knfmt-options]
#
# Run test case.
testcase() {
	local _err=0
	local _bug=0
	local _quiet=0
	local _file

	while [ $# -gt 0 ]; do
		case "$1" in
		-b)	_bug=1; _quiet=1;;
		-q)	_quiet=1;;
		*)	break;;
		esac
		shift
	done
	_file="$1"; : "${_file:?}"; shift
	[ "${1:-}" = "--" ] && shift

	_ok="${_file%.c}.ok"
	if [ -e "$_ok" ]; then
		_tmp="${_wrkdir}/tmp"
		commstrip "$_file" >"$_tmp"
		if ! ${EXEC:-} "${KNFMT}" -v "$@" "$_tmp" 2>&1 |
			diff -u -L "$_file" -L "$_ok" "$_ok" - >"$_out" 2>&1
		then
			cat "$_out" 1>&2
			return 1
		fi
	else
		${EXEC:-} "${KNFMT}" -dv "$@" "$_file" >"$_out" 2>&1 || _err="$?"
		if [ "$_err" -ne 0 ]; then
			if [ "$_bug" -eq 1 ] && [ "$_err" -eq 1 ]; then
				:
			else
				cat "$_out" 1>&2
				return 1
			fi
		fi
	fi
	if [ "$_quiet" -eq 0 ] && ! cmp -s /dev/null "$_out"; then
		cat "$_out" 1>&2
		return 1
	fi
}

# Enable hardening malloc(3) options on OpenBSD.
case "$(uname -s)" in
OpenBSD)	export MALLOC_OPTIONS="RS";;
esac

# Use a distinct ASan exit code.
export ASAN_OPTIONS="exitcode=66"

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap "rm -rf ${_wrkdir}" 0
_out="${_wrkdir}/out"

case "$1" in
valid-183.c)
	# Ignore windows line endings test case.
	;;
bug-*|error-*|simple-*|valid-*)
	hascomm "$1" || exit 1
	;;
esac

case "$1" in
bug-*)
	testcase -b "$1" -- -s
	;;
diff-*)
	_base="${1%.c}"
	_ok="${_base}.ok"
	if ! ${EXEC:-} ${KNFMT} -D <"${_base}.patch" 2>&1 | \
		diff -u -L "$1" -L "$_ok" "$_ok" - >"$_out" 2>&1
	then
		cat "$_out" 1>&2
		exit 1
	fi
	;;
error-*)
	_err=0
	${EXEC:-} ${KNFMT} -s "$1" >"$_out" 2>&1 || _err="$?"
	if [ "$_err" -ne 1 ]; then
		cat "$_out" 1>&2
		echo "${1}; expected exit 1, got ${_err}" 1>&2
		exit 1
	fi
	;;
simple-*|../*)
	testcase "$1" -- -s
	;;
trace-*)
	testcase -q "$1" -- -vvv
	;;
*)
	testcase "$1"
	;;
esac

#!/bin/sh

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

# testcase [-b] [-e] [-q] file [-- knfmt-options]
#
# Run test case.
testcase() {
	local _bug=0
	local _diff
	local _exp=0
	local _file
	local _flags="d"
	local _got=0
	local _quiet=0

	while [ $# -gt 0 ]; do
		case "$1" in
		-b)	_bug=1; _quiet=1;;
		-e)	_exp=1; _flags="";;
		-q)	_quiet=1;;
		*)	break;;
		esac
		shift
	done
	_file="$1"; : "${_file:?}"; shift
	[ "${1:-}" = "--" ] && shift

	_ok="${_file%.c}.ok"
	_patch="${_file%.c}.patch"
	if [ -e "$_patch" ]; then
		if ! ${EXEC:-} "${KNFMT}" "$@" <"$_patch" 2>&1 |
			diff -u -L "$_ok" -L "$_file" "$_ok" - >"$_out" 2>&1
		then
			cat "$_out" 1>&2
			return 1
		fi
	elif [ -e "$_ok" ]; then
		_tmp="${_wrkdir}/test.c"
		_diff="${_wrkdir}/diff"
		commstrip "$_file" >"$_tmp"
		(cd "$_wrkdir" && ${EXEC:-} "${KNFMT}" "$@" test.c) \
			>"$_diff" 2>&1 || _got="$?"
		if ! diff -u -L "$_ok" -L "$_file" "$_ok" "$_diff" >"$_out" 2>&1; then
			cat "$_out" 1>&2
			return 1
		fi
	else
		(cd "$_wrkdir" && ${EXEC:-} "${KNFMT}" ${_flags:+-${_flags}} "$@" "$_file") \
			>"$_out" 2>&1 || _got="$?"
	fi

	if [ "$_exp" -ne "$_got" ]; then
		if [ "$_bug" -eq 1 ] && [ "$_got" -ne 0 ]; then
			:
		elif [ "$_exp" -eq 1 ]; then
			cat "$_out" /dev/stdin 1>&2 <<-EOF
			${_file}: expected exit 1, got ${_got}
			EOF
			return 1
		else
			cat "$_out" 1>&2
			return 1
		fi
	elif [ "$_exp" -eq 1 ] && ! [ -e "$_ok" ] && cmp -s - /dev/null <"$_out"; then
		echo "${_file}: expected error message" 1>&2
		return 1
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
trap 'rm -rf ${_wrkdir}' 0
_out="${_wrkdir}/out"

# Ensure presence of test case description.
case "$1" in
valid-183.c)
	# Ignore windows line endings test case.
	;;
bug-*|error-*|simple-*|valid-*)
	hascomm "$1" || exit 1
	;;
esac

# Specific exceptions.
case "$1" in
simple-010.c)
	# Sort order discrepancy.
	[ "${MUSL:-0}" -eq 1 ] && exit 0
	;;
*)
	;;
esac

_rel="$1"
_abs="$(readlink -f "$_rel")"
case "$_rel" in
bug-*)
	testcase -b "$_abs" -- -sv
	;;
diff-simple-*)
	testcase "$_rel" -- -Dsv
	;;
diff-*)
	testcase "$_rel" -- -Dv
	;;
error-*)
	testcase -e -q "$_abs" -- -s
	;;
simple-*|../*)
	testcase "$_abs" -- -sv
	;;
trace-*)
	testcase -q "$_abs" -- -vvv
	;;
*)
	testcase "$_abs" -- -v
	;;
esac

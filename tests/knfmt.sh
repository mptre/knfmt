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
		if ! ${EXEC:-} "${KNFMT}" -v "$@" <"$_patch" 2>&1 |
			diff -u -L "$_ok" -L "$_file" "$_ok" - >"$_out" 2>&1
		then
			cat "$_out" 1>&2
			return 1
		fi
	elif [ -e "$_ok" ]; then
		_tmp="${_wrkdir}/tmp"
		commstrip "$_file" >"$_tmp"
		if ! ${EXEC:-} "${KNFMT}" -v "$@" "$_tmp" 2>&1 |
			diff -u -L "$_ok" -L "$_file" "$_ok" - >"$_out" 2>&1
		then
			cat "$_out" 1>&2
			return 1
		fi
	else
		${EXEC:-} "${KNFMT}" "-${_flags}v" "$@" "$_file" \
			>"$_out" 2>&1 || _got="$?"
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
		elif [ "$_exp" -eq 1 ] && cmp -s - /dev/null <"$_out"; then
			echo "${_file}: expected error message" 1>&2
			return 1
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

case "$1" in
bug-*)
	testcase -b "$1" -- -s
	;;
diff-simple-*)
	testcase "$1" -- -Ds
	;;
diff-*)
	testcase "$1" -- -D
	;;
error-*)
	testcase -e -q "$1" -- -s
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

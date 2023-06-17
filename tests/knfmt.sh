#!/bin/sh

set -e

# commstrip file
#
# Removed the leading block comment from file.
commstrip() {
	{ while read -r _l; do [ -n "$_l" ] || break; done; cat; } <"$1"
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

# testcase [-b] [-c] [-e] [-i] [-q] file [-- knfmt-options]
#
# Run test case.
testcase() {
	local _base
	local _bug=0
	local _clang=0
	local _diff="${_wrkdir}/diff"
	local _exp=0
	local _file
	local _flags="d"
	local _got=0
	local _quiet=0

	while [ $# -gt 0 ]; do
		case "$1" in
		-b)	_bug=1; _quiet=1;;
		-c)	_clang=1;;
		-e)	_exp=1; _flags="";;
		-i)	_flags=""; _quiet=1;;
		-q)	_quiet=1;;
		*)	break;;
		esac
		shift
	done
	_file="$1"; : "${_file:?}"; shift
	[ "${1:-}" = "--" ] && shift

	_name="${_file##*/}"
	_base="${_name%.c}"
	_ok="${_file%.c}.ok"
	_patch="${_file%.c}.patch"

	if [ "$_clang" -eq 1 ]; then
		sed -n -e '/^[\/ ]\* /s/^[\/ ]\* //p' -e '/^$/q' "$_file" |
		sed -e '/^$/d' >"${_wrkdir}/.clang-format"
	fi

	if [ "$_bug" -eq 1 ] && ! [ -e "$_ok" ]; then
		echo "${_ok}: file not found" 1>&2
		return 1
	fi

	if [ -e "$_patch" ]; then
		if [ "$_clang" -eq 1 ]; then
			commstrip "$_file" >"${_wrkdir}/${_name}"
		else
			cp "$_file" "${_wrkdir}/${_name}"
		fi
		cp "$_ok" "${_wrkdir}/${_base}.ok"
		(cd "$_wrkdir" && ${EXEC:-} "${KNFMT}" "$@" <"$_patch") \
			>"$_diff" 2>&1 || _got="$?"
		if ! diff -u -L "${_base}.ok" -L "$_name" "$_ok" "$_diff" >"$_out" 2>&1; then
			cat "$_out" 1>&2
			return 1
		fi
	elif [ -e "$_ok" ]; then
		_tmp="${_wrkdir}/test.c"
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
*)		;;
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
diff-*|inplace-*|../*)
	;;
*)
	hascomm "$1" || exit 1
	;;
esac

_path="$1"; shift

_rel="$_path"
_abs="$(readlink -f "$_rel" 2>/dev/null || echo "${PWD}/${_rel}")"
case "$_rel" in
bug-*)
	testcase -b "$_abs" -- -s "$@"
	;;
diff-simple-*)
	testcase "$_abs" -- -Ds -tl "$@"
	;;
diff-style-*)
	testcase -c "$_abs" -- -Ds -tl "$@"
	;;
diff-*)
	testcase "$_abs" -- -D -tl "$@"
	;;
error-*)
	testcase -e -q "$_abs" -- -s "$@"
	;;
inplace-*)
	cp "$_abs" "${_wrkdir}/test.c"
	testcase -i "${_wrkdir}/test.c" -- -i "$@"
	;;
simple-*|../*)
	testcase "$_abs" -- -s -tl "$@"
	;;
style-bug-*)
	testcase -b -c "$_abs" -- -ts "$@"
	;;
style-error-*)
	testcase -c "$_abs" -- -ts "$@"
	;;
style-simple-*)
	testcase -c "$_abs" -- -s -tls "$@"
	;;
style-*)
	testcase -c "$_abs" -- -tls "$@"
	;;
trace-*)
	testcase -q "$_abs" -- -s -ta "$@"
	;;
*)
	testcase "$_abs" -- -tl "$@"
	;;
esac

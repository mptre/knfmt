#!/bin/sh

set -e

# commstrip file
#
# Removed the leading block comment from file.
commstrip() {
	{ while read -r _l; do [ -n "$_l" ] || break; done; cat; } <"$1"
}

# clang_format_stub
clang_format_stub() {
	local _bindir="${_wrkdir}/bin"

	mkdir -p "$_bindir"
	cat <<-EOF >"${_bindir}/clang-format"
	#!/bin/sh

	_style=""
	for _a; do
		case "\$_a" in
		-style=*)	_style=\${_a#*=};;
		esac
	done
	cd ${PWD}
	cat styles/\${_style}
	EOF
	chmod u+x "${_bindir}/clang-format"

	PATH="${_bindir}:${PATH}"; export PATH
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

# testcase [-B] [-b] [-c] [-e] [-i] [-o] [-q] file [-- knfmt-options]
#
# Run test case.
testcase() {
	local _base
	local _based_on_style=0
	local _bug=0
	local _clang=0
	local _diff="${_wrkdir}/diff"
	local _exp=0
	local _ext
	local _file
	local _flags="d"
	local _got=0
	local _ok=
	local _ok_required=0
	local _quiet=0

	while [ $# -gt 0 ]; do
		case "$1" in
		-B)	_based_on_style=1;;
		-b)	_bug=1; _quiet=1;;
		-c)	_clang=1;;
		-e)	_exp=1; _flags="";;
		-i)	_flags=""; _quiet=1;;
		-o)	_ok_required=1;;
		-q)	_quiet=1;;
		*)	break;;
		esac
		shift
	done
	_file="$1"; : "${_file:?}"; shift
	[ "${1:-}" = "--" ] && shift

	_name="${_file##*/}"
	_ext=".${_name##*.}"
	_base="${_name%"${_ext}"}"
	_ok="${_base}.ok"
	_patch="${_file%.*}.patch"

	if [ "$_clang" -eq 1 ]; then
		sed -n -e '/^[\/ ]\* /s/^[\/ ]\* //p' -e '/^$/q' "$_file" |
		sed -e '/^$/d' >"${_wrkdir}/.clang-format"
	fi

	if [ "$_based_on_style" -eq 1 ]; then
		clang_format_stub
	fi

	if [ "$_ok_required" -eq 1 ] && ! [ -e "$_ok" ]; then
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
		mkdir "${_wrkdir}/tests"
		_tmp="${_wrkdir}/tests/test${_ext}"
		commstrip "$_file" >"$_tmp"
		(cd "${_wrkdir}/tests" && ${EXEC:-} "${KNFMT}" "$@" "test${_ext}") \
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
	testcase -b -o "$_abs" -- -s "$@"
	;;
diff-simple-*)
	testcase "$_abs" -- -Ds "$@"
	;;
diff-style-*)
	testcase -c "$_abs" -- -Ds "$@"
	;;
diff-*)
	testcase "$_abs" -- -D "$@"
	;;
error-*)
	testcase -e -q "$_abs" -- -s "$@"
	;;
inplace-*)
	cp "$_abs" "${_wrkdir}/test.c"
	testcase -i "${_wrkdir}/test.c" -- -i "$@"
	;;
simple-*|../*)
	testcase "$_abs" -- -s "$@"
	;;
style-BasedOnStyle-*)
	testcase -B -c "$_abs" -- -ts "$@"
	;;
style-bug-*)
	testcase -b -c -o "$_abs" -- -ts "$@"
	;;
style-error-*)
	testcase -c "$_abs" -- -ts "$@"
	;;
style-simple-*)
	testcase -c "$_abs" -- -s -ts "$@"
	;;
style-trace-*)
	testcase -c -o "$_abs" -- -s -tss "$@"
	;;
style-*)
	testcase -c "$_abs" -- -ts "$@"
	;;
trace-*)
	testcase -c -q "$_abs" -- -s -ta "$@"
	;;
*)
	testcase "$_abs" -- "$@"
	;;
esac

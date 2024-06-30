#!/bin/sh
#
# The cp.sh utility is used to find the next available test case path of a
# certain type, expressed as a file name prefix.

set -eu

usage() {
	echo "usage: sh cp.sh [-n] path type" 1>&2
	exit 1
}

# next dir type
next() {
	local _c
	local _dir
	local _p=0
	local _type

	_dir="$1"; : "${_dir:?}"
	_type="$2"; : "${_type:?}"

	# shellcheck disable=SC2010
	(cd "${_dir}" && ls | grep "^${_type}-...\.[ch]$" 2>/dev/null) |
	sed 's/.*-0*\([0-9]*\)\.c/\1/' |
	sort -n |
	tee "${TMP}" |
	while read -r _c; do
		if [ $((_c - _p)) -gt 1 ]; then
			echo "$((_p + 1))"
			return 1
		fi
		_p="${_c}"
	done || return 0

	_c="$(tail -1 "${TMP}")"
	echo $((_c + 1))
}

_exec="eval"

while getopts "n" _opt; do
	case "${_opt}" in
	n)	_exec="echo";;
	*)	usage;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 2 ] || usage
_path="$1"
_type="$2"

TMP="$(mktemp -t knfmt.XXXXXX)"
trap 'rm $TMP' 0

_n="$(next tests "${_type}")"
"${_exec}" "$(printf 'mv %s tests/%s-%03d.c\n' "${_path}" "${_type}" "${_n}")"

#/bin/sh

set -e

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
	"${KNFMT}" -d "$1"
	;;
esac

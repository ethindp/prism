#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
platform="${1:?platform}"
arch="${2:?arch}"
target="${3:?target-type}"
outdir="gdextension/project/bin/${platform}"
case "${platform}" in
windows) ext="dll" ;;
macos | ios) ext="dylib" ;;
*) ext="so" ;;
esac
expected="prismatoid.${platform}.${target}.${arch}.${ext}"
actual="$(find "${outdir}" -type f 2>/dev/null | head -1)"
if [[ -z "${actual}" ]]; then
	echo "No binary found in ${outdir}" >&2
	exit 1
fi
actual_name="$(basename "${actual}")"
if [[ "${actual_name}" = "${expected}" ]]; then
	echo "Binary name correct: ${actual_name}"
else
	echo "Renaming ${actual_name} to ${expected}"
	mv "${actual}" "${outdir}/${expected}"
fi

#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
EM_SYSROOT="$(em-config CACHE)/sysroot"
[[ -d "${EM_SYSROOT}" ]] || {
	echo "no emscripten sysroot at ${EM_SYSROOT}" >&2
	exit 1
}
TIDY_FLAGS="clang-tidy;--extra-arg=--sysroot=${EM_SYSROOT};--extra-arg=-stdlib=libc++"
CMAKE_COMMON=(
	-G Ninja
	-DPRISM_ENABLE_TESTS=OFF
	-DPRISM_ENABLE_DEMOS=OFF
	-DPRISM_ENABLE_LEGACY_BACKENDS=ON
	-DPRISM_ENABLE_SHIMS=ON
	-DPRISM_ENABLE_LINTING=ON
	-DPRISM_ENABLE_GDEXTENSION=OFF
)
for bt in Release Debug; do
	config=$(echo "${bt}" | tr '[:upper:]' '[:lower:]')
	dir="build-${config}"
	echo "::group::lint ${dir}"
	emcmake cmake -B "${dir}" "${CMAKE_COMMON[@]}" \
		-DCMAKE_BUILD_TYPE="${bt}" \
		-DCMAKE_C_CLANG_TIDY="${TIDY_FLAGS}" \
		-DCMAKE_CXX_CLANG_TIDY="${TIDY_FLAGS}"
	cmake --build "${dir}"
	echo "::endgroup::"
done

#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
WINELIBS="${PRISM_BUILD_WINELIBS:-ON}"
CMAKE_COMMON=(
	-G Ninja
	-DPRISM_ENABLE_TESTS=OFF
	-DPRISM_ENABLE_DEMOS=OFF
	-DPRISM_ENABLE_LEGACY_BACKENDS=ON
	-DPRISM_ENABLE_SHIMS=ON
	-DPRISM_ENABLE_LINTING=ON
	-DPRISM_ENABLE_GDEXTENSION=OFF
	"-DPRISM_BUILD_WINELIBS=${WINELIBS}"
)
for shared in ON OFF; do
	linkage=$([[ "${shared}" = "ON" ]] && echo dynamic || echo static)
	for bt in Release Debug; do
		config=$(echo "${bt}" | tr '[:upper:]' '[:lower:]')
		dir="build-${linkage}-${config}"
		echo "::group::lint ${dir}"
		cmake -B "${dir}" "${CMAKE_COMMON[@]}" \
			-DCMAKE_BUILD_TYPE="${bt}" \
			-DCMAKE_C_COMPILER=clang \
			-DCMAKE_CXX_COMPILER=clang++ \
			-DCMAKE_C_CLANG_TIDY=clang-tidy \
			-DCMAKE_CXX_CLANG_TIDY=clang-tidy \
			-DBUILD_SHARED_LIBS="${shared}"
		cmake --build "${dir}" --target prism
		echo "::endgroup::"
	done
done

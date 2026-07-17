#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
CMAKE_COMMON=(
	-G Ninja
	-DPRISM_ENABLE_TESTS=OFF
	-DPRISM_ENABLE_DEMOS=OFF
	-DPRISM_ENABLE_LEGACY_BACKENDS=ON
	-DPRISM_ENABLE_SHIMS=ON
	-DPRISM_ENABLE_GDEXTENSION=OFF
)
for bt in Release Debug; do
	config=$(echo "${bt}" | tr '[:upper:]' '[:lower:]')
	dir="build-${config}"
	echo "::group::build ${dir}"
	emcmake cmake -B "${dir}" "${CMAKE_COMMON[@]}" -DCMAKE_BUILD_TYPE="${bt}"
	cmake --build "${dir}"
	echo "::endgroup::"
done

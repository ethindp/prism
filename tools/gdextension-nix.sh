#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
platform="${1:?platform}"
arch="${2:?arch}"
target_type="${3:?target-type}"
lint="${PRISM_GDEXT_LINT:-OFF}"
: "${PRISM_GODOT_CPP_SOURCE_DIR:?not set -- are you inside a nix develop shell?}"
if [[ "${target_type}" = "template_release" ]]; then
	bt="Release"
else
	bt="Debug"
fi
args=(
	-B build -G Ninja
	-DPRISM_ENABLE_TESTS=OFF
	-DPRISM_ENABLE_DEMOS=OFF
	-DPRISM_ENABLE_LEGACY_BACKENDS=ON
	-DPRISM_ENABLE_SHIMS=OFF
	-DCMAKE_BUILD_TYPE="${bt}"
	-DBUILD_SHARED_LIBS=OFF
	-DPRISM_ENABLE_GDEXTENSION=ON
	-DGODOTCPP_TARGET="${target_type}"
	-DPRISM_GODOT_CPP_SOURCE_DIR="${PRISM_GODOT_CPP_SOURCE_DIR}"
)
if [[ "${lint}" = "ON" ]]; then
	args+=(-DPRISM_ENABLE_LINTING=ON)
fi
case "${platform}-${arch}" in
linux-x86_64 | linux-arm64)
	cmake "${args[@]}"
	;;
web-wasm32)
	emcmake cmake "${args[@]}"
	;;
*)
	echo "gdextension-nix.sh does not serve ${platform}-${arch}." >&2
	echo "Use tools/gdextension-native.sh." >&2
	exit 1
	;;
esac
cmake --build build --target prismatoid

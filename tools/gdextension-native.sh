#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
platform="${1:?platform}"
arch="${2:?arch}"
target_type="${3:?target-type}"
lint="${PRISM_GDEXT_LINT:-OFF}"
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
)
[[ "${lint}" = "ON" ]] && args+=(-DPRISM_ENABLE_LINTING=ON)
use_emcmake=false
case "${platform}-${arch}" in
linux-x86_32)
	args+=(-DCMAKE_C_FLAGS="-m32" -DCMAKE_CXX_FLAGS="-m32"
		-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=x86_32)
	;;
linux-arm32)
	args+=(-DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc
		-DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
		-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm32)
	;;
windows-*)
	if [[ "${lint}" = "ON" ]]; then
		args+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
	fi
	case "${arch}" in
	x86_32) args+=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_32) ;;
	arm64) args+=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=arm64) ;;
	esac
	;;
macos-arm64)
	args+=(-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0)
	;;
macos-x86_64)
	args+=(-DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
		-DCMAKE_Swift_COMPILER_TARGET=x86_64-apple-macosx15.0)
	;;
android-*)
	: "${ANDROID_NDK_HOME:?not set -- the NDK setup step must run first}"
	declare -A abi_map=([arm64]=arm64-v8a [arm32]=armeabi-v7a
		[x86_64]=x86_64 [x86_32]=x86)
	args+=("-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"
		"-DANDROID_ABI=${abi_map[${arch}]}"
		-DANDROID_PLATFORM=android-21)
	;;
ios-arm64)
	SDKROOT="$(xcrun --sdk iphoneos --show-sdk-path)"
	[[ -d "${SDKROOT}" ]] || {
		echo "iOS SDK not found via xcrun" >&2
		exit 1
	}
	export SDKROOT
	args+=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64
		-DCMAKE_OSX_SYSROOT="${SDKROOT}" -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0)
	;;
linux-x86_64 | linux-arm64 | web-wasm32)
	echo "${platform}-${arch} is served by tools/gdextension-nix.sh, not this script." >&2
	exit 1
	;;
*)
	echo "unknown target ${platform}-${arch}" >&2
	exit 1
	;;
esac
if [[ "${use_emcmake}" = "true" ]]; then
	emcmake cmake "${args[@]}"
else
	cmake "${args[@]}"
fi
cmake --build build --target prismatoid

#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
for config in release debug; do
	bt=$([[ "${config}" = "release" ]] && echo Release || echo Debug)
	rm -rf "dist/static/${config}"
	cmake --install "build-${config}" --prefix "dist/static/${config}" --config "${bt}"
done
mv dist/static/release/include dist/include
rm -rf dist/static/debug/include
cp -r LICENSES dist
cp NOTICE dist
cd dist && zip -r ../prism-wasm.zip .

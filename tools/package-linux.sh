#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
ARCH="${1:?usage: $0 <arch-name>}"
for linkage in dynamic static; do
	for config in release debug; do
		bt=$([[ "${config}" = "release" ]] && echo Release || echo Debug)
		rm -rf "dist/${linkage}/${config}"
		cmake --install "build-${linkage}-${config}" \
			--prefix "dist/${linkage}/${config}" --config "${bt}"
	done
done
mv dist/dynamic/release/include dist/include
rm -rf dist/dynamic/debug/include dist/static/release/include dist/static/debug/include
cp -r LICENSES dist
cp NOTICE dist
cd dist && zip -r "../prism-linux-${ARCH}.zip" .

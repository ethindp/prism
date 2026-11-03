#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -euo pipefail
ARCH="${1:?usage: $0 <arch-name>}"
for linkage in dynamic static; do
	for config in release debug; do
		bt=$([[ ${config} == "release" ]] && echo Release || echo Debug)
		rm -rf "dist/${linkage}/${config}"
		cmake --install "build-${linkage}-${config}" \
			--prefix "dist/${linkage}/${config}" --config "${bt}"
	done
done
cp -r dist/dynamic/release/include dist/include
cp -r LICENSES dist
cp NOTICE dist
cd dist && zip -r "../prism-linux-${ARCH}.zip" .

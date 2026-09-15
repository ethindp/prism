// SPDX-License-Identifier: MPL-2.0

use std::env;
use std::path::{Path, PathBuf};

fn main() {
    println!("cargo:rerun-if-env-changed=PRISM_LIB_DIR");
    println!("cargo:rerun-if-env-changed=PRISM_STATIC");

    if env::var("CARGO_FEATURE_MOCK").is_ok() {
        return;
    }

    if let Ok(dir) = env::var("PRISM_LIB_DIR") {
        println!("cargo:rustc-link-search=native={dir}");
    } else {
        // Probe common in-tree Prism build and distribution directories
        let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        let repo_root = manifest_dir.join("../..");

        let candidate_dirs = [
            repo_root.join("build"),
            repo_root.join("build/Release"),
            repo_root.join("build/Debug"),
            repo_root.join("build-dynamic-release"),
            repo_root.join("build-static-release"),
            repo_root.join("dist/dynamic/release"),
            repo_root.join("dist/static/release"),
            repo_root.join("dist"),
        ];

        for dir in &candidate_dirs {
            if dir.is_dir() && contains_prism_library(dir) {
                println!("cargo:rustc-link-search=native={}", dir.display());
                break;
            }
        }
    }

    let is_static = env::var("PRISM_STATIC")
        .map(|v| v == "1" || v == "ON")
        .unwrap_or(false);
    if is_static {
        println!("cargo:rustc-link-lib=static=prism");
    } else {
        println!("cargo:rustc-link-lib=prism");
    }
}

fn contains_prism_library(dir: &Path) -> bool {
    let names = [
        "prism.lib",
        "prism.dll",
        "libprism.so",
        "libprism.dylib",
        "libprism.a",
    ];
    names.iter().any(|name| dir.join(name).exists())
}

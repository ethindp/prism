use std::env;
use std::fs;
use std::io::Cursor;
use std::path::{Path, PathBuf};
use std::process::Command;

const REPO_ZIP_URL: &str = "https://github.com/ethindp/prism/archive/refs/tags/v0.3.0-aa15fe7.zip";
const REPO_ROOT_NAME: &str = "prism-0.3.0";

fn main() -> anyhow::Result<()> {
    let out_dir = PathBuf::from(env::var("OUT_DIR")?);
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR")?);
    let target_os = env::var("CARGO_CFG_TARGET_OS")?;
    let local_src = manifest_dir.join("../../../");
    let source_dir = if local_src.join("CMakeLists.txt").exists() {
        dunce::canonicalize(local_src)?
    } else {
        download_and_extract_repo(REPO_ZIP_URL, &out_dir, REPO_ROOT_NAME)?
    };
    let mut config = cmake::Config::new(&source_dir);
    config.define("PRISM_ENABLE_TESTS", "OFF"); // Disable tests as requested
    config.define("PRISM_ENABLE_DEMOS", "OFF"); // Disable demos as requested    
    let profile = env::var("PROFILE").unwrap();
    let build_type = if profile == "release" {
        "Release"
    } else {
        "Debug"
    };
    config.profile(build_type);
    let dst = config.build();
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=prism");
    let bin_path = if target_os == "windows" {
        dst.join("bin")
    } else {
        dst.join("lib")
    };
    if bin_path.exists() {
        copy_runtime_artifacts(&bin_path, &out_dir)?;
    }
    let header_path = source_dir.join("include").join("prism.h");
    println!("cargo:rerun-if-changed={}", header_path.display());
    let bindings = bindgen::Builder::default()
        .header(header_path.to_string_lossy())
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");
    bindings
        .write_to_file(out_dir.join("bindings.rs"))
        .expect("Couldn't write bindings!");
    Ok(())
}

fn copy_runtime_artifacts(src_dir: &Path, out_dir: &Path) -> anyhow::Result<()> {
    let mut target_dir = out_dir.to_path_buf();
    if target_dir.pop() && target_dir.pop() && target_dir.pop() {
        for entry in walkdir::WalkDir::new(src_dir) {
            let entry = entry?;
            let path = entry.path();
            if let Some(ext) = path.extension() {
                let ext = ext.to_string_lossy().to_lowercase();
                if matches!(ext.as_str(), "dll" | "so" | "dylib" | "pdb") {
                    let file_name = path.file_name().unwrap();
                    let dest = target_dir.join(file_name);
                    fs::copy(path, &dest)?;
                }
            }
        }
    }
    Ok(())
}

fn download_and_extract_repo(url: &str, dest: &Path, root_name: &str) -> anyhow::Result<PathBuf> {
    let extract_path = dest.join("downloaded_src");
    let final_path = extract_path.join(root_name);
    if final_path.exists() {
        return Ok(final_path);
    }
    let resp = reqwest::blocking::get(url)?.error_for_status()?;
    let content = Cursor::new(resp.bytes()?);
    let mut archive = zip::ZipArchive::new(content)?;
    archive.extract(&extract_path)?;
    Ok(final_path)
}

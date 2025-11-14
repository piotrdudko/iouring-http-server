use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    // Get the path to the clib directory
    let clib_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
        .parent()
        .unwrap()
        .join("clib");

    // Build the C library using its Makefile
    let status = Command::new("make")
        .current_dir(&clib_dir)
        .status()
        .expect("Failed to build C library");

    if !status.success() {
        panic!("Failed to build C library");
    }

    // Tell cargo to link the C library
    println!("cargo:rustc-link-search=native={}", clib_dir.display());
    println!("cargo:rustc-link-lib=static=iouring_helpers");
    println!("cargo:rustc-link-lib=uring");

    // Generate bindings using bindgen
    let bindings = bindgen::Builder::default()
        .header(clib_dir.join("wrapper.h").to_str().unwrap())
        .clang_arg(format!("-I{}", clib_dir.display()))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    // Write the bindings to $OUT_DIR/bindings.rs
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    // Tell cargo to rerun if the C library changes
    println!("cargo:rerun-if-changed={}/wrapper.h", clib_dir.display());
    println!("cargo:rerun-if-changed={}/bufring.h", clib_dir.display());
    println!("cargo:rerun-if-changed={}/logging.h", clib_dir.display());
    println!("cargo:rerun-if-changed={}/userdata.h", clib_dir.display());
    println!("cargo:rerun-if-changed={}/errors.h", clib_dir.display());
}

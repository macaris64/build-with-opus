fn main() {
    // Re-run this build script if the mission config header changes
    println!("cargo:rerun-if-changed=../../_defs/mission_config.h");

    let bindings = bindgen::Builder::default()
        .header("../../_defs/mission_config.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("bindgen failed to generate bindings from mission_config.h");

    let out_path = std::path::PathBuf::from(
        std::env::var("OUT_DIR").expect("OUT_DIR not set by cargo"),
    );
    bindings
        .write_to_file(out_path.join("mission_config_bindings.rs"))
        .expect("failed to write bindings file");
}

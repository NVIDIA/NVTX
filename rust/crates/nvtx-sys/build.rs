use std::{
    env,
    path::{Path, PathBuf},
};

use bindgen::RustTarget;

fn main() {
    let mut lib_builder = cc::Build::new();
    let mut builder = bindgen::Builder::default();

    lib_builder
        .include("vendor/nvtx/c/include")
        .include("c/include")
        .opt_level(2)
        .file(Path::new("c/src/lib.c"));

    builder = builder
        .rust_target(
            #[allow(deprecated)]
            RustTarget::Stable_1_77,
        ) // MSRV
        .detect_include_paths(true)
        .clang_arg("-I")
        .clang_arg("vendor/nvtx/c/include")
        .clang_arg("-I")
        .clang_arg("c/include")
        .header("c/include/wrapper.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .allowlist_recursively(false)
        .generate_comments(false)
        .generate_cstr(true)
        .default_alias_style(bindgen::AliasVariation::TypeAlias)
        .default_enum_style(bindgen::EnumVariation::Rust {
            non_exhaustive: false,
        })
        .constified_enum_module("nvtxResource.*_t")
        .c_naming(false)
        .default_macro_constant_type(bindgen::MacroTypeVariation::Signed)
        .sort_semantically(true)
        .translate_enum_integer_types(true)
        .wrap_unsafe_ops(true)
        // mark any nvxt(...)_t type as required except those starting with nvtxRes
        .must_use_type("nvtx[^R][^e][^s].*_t")
        // permit all nvtx-prefixed types except internal ones
        .allowlist_type("nvtx[^_].*")
        // expose NVTX_VERSION
        .allowlist_var("NVTX_VERSION")
        // expose all nvtx-prefixed functions
        .allowlist_function("nvtx.*")
        // expose wchar_t for wide function parameters
        .allowlist_type("wchar_t")
        // allow cuda types
        .allowlist_type("CU.*")
        .allowlist_type("cuda.*")
        // disallow any fntypes
        .blocklist_type(".*fntype.*")
        // disallow impl-specific
        .blocklist_type("__.*");

    if cfg!(feature = "cuda") {
        builder = builder.clang_arg("-DENABLE_CUDA");
        lib_builder.define("ENABLE_CUDA", None);
    }
    if cfg!(feature = "cuda_runtime") {
        builder = builder.clang_arg("-DENABLE_CUDART");
        lib_builder.define("ENABLE_CUDART", None);
    }

    lib_builder.compile("nvtx");
    let bindings = builder.generate().expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

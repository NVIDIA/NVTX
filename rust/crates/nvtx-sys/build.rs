// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#![allow(clippy::unwrap_used)]
#![allow(clippy::expect_used)]

use std::{
    env,
    path::{Path, PathBuf},
};

fn main() {
    let mut lib_builder = cc::Build::new();
    let mut builder = bindgen::Builder::default();
    let host = env::var("HOST").expect("host triple is always set");

    lib_builder
        .include("../../../c/include")
        .include("c/include")
        .opt_level(2)
        .file(Path::new("c/src/lib.c"));

    builder = builder
        .use_core()
        .detect_include_paths(true)
        .clang_arg("-I")
        .clang_arg("../../../c/include")
        .clang_arg("-I")
        .clang_arg("c/include")
        .header("c/include/wrapper.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Bare-metal checks parse NVTX headers using the host target, so bindgen's
        // generated host layout assertions are not meaningful for that target.
        .layout_tests(false)
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
        // mark any nvtx(...)_t type as required except those starting with nvtxRes
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
        // disallow impl-specific
        .blocklist_type("__.*");

    if cfg!(feature = "tools") {
        builder = builder
            // expose the callback-subscription types (export tables, callback
            // modules, and callback ids)
            .allowlist_type("Nvtx.*")
            // expose the dependency-free fake types referenced by the
            // fakeimpl fntypes (required because allowlisting is not recursive)
            .allowlist_type("nvtx_.*")
            // expose the injection result codes
            .allowlist_var("NVTX_SUCCESS")
            .allowlist_var("NVTX_FAIL")
            .allowlist_var("NVTX_ERR_.*");
        // the function-table slot signatures (`*_impl_fntype` and
        // `*_fakeimpl_fntype`) are covered by the "nvtx[^_].*" allowlist above
    } else {
        // disallow any fntypes
        builder = builder.blocklist_type(".*fntype.*");
    }

    if cfg!(feature = "cuda") {
        builder = builder.clang_arg("-DENABLE_CUDA");
        lib_builder.define("ENABLE_CUDA", None);
    }
    if cfg!(feature = "cuda_runtime") {
        builder = builder.clang_arg("-DENABLE_CUDART");
        lib_builder.define("ENABLE_CUDART", None);
    }

    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("target OS is always set");
    if target_os == "none" {
        // For bare-metal cross checks, parse headers against the host's libc headers.
        // NVTX does not expose target-dependent structs, so host parsing is sufficient.
        builder = builder.clang_arg(format!("--target={host}"));
        println!("cargo:warning=Skipping C shim build for target OS 'none'");
    } else {
        lib_builder.compile("nvtx");
    }
    let bindings = builder.generate().expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.txt for license information.
 */

fn main() {
    extern crate bindgen;

    use std::env;
    use std::path::PathBuf;

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    println!("cargo:rerun-if-changed=src/nvtxw-sys/include/nvtxw3/nvtxw3.h");
    println!("cargo:rerun-if-changed=src/nvtxw-sys/src/nvtxw3/nvtxw3.c");

    let bindings = bindgen::Builder::default()
        .header("src/nvtxw-sys/include/nvtxw3/nvtxw3.h")
        .clang_arg("-Isrc/nvtxw-sys/include")
        .allowlist_item("nvtx.*")
        .allowlist_item("NVTX.*")
        .default_macro_constant_type(bindgen::MacroTypeVariation::Signed)
        .derive_default(true)
        .generate()
        .expect("Unable to generate bindings");

    bindings
        .write_to_file(out_path.join("nvtxw_bindings.rs"))
        .expect("Unable to write bindings");

    cc::Build::new()
        .include("src/nvtxw-sys/include")
        .file("src/nvtxw-sys/src/nvtxw3/nvtxw3.c")
        .compile("nvtxw-sys");
}

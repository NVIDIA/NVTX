#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -eux

cargo fmt --all -- --check

run_profile() {
    cargo check --workspace --all-targets "$@"
    cargo clippy --workspace --all-targets "$@" -- -Dwarnings
    cargo test --workspace --all-targets "$@"
    cargo test --workspace --doc "$@"
}

run_profile --all-features
run_profile --no-default-features --features alloc
run_profile --no-default-features

rustup target add thumbv7em-none-eabi
cargo check --workspace --no-default-features --target thumbv7em-none-eabi
cargo check --workspace --no-default-features --features alloc --target thumbv7em-none-eabi

cargo install --locked cargo-deny || true
cargo deny --workspace --all-features check --show-stats

cargo +1.77.0 check --workspace --all-targets --all-features

cargo install --locked cargo-toml-lint || true
cargo-toml-lint Cargo.toml
cargo-toml-lint crates/nvtx-sys/Cargo.toml

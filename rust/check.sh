#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -eux

cargo fmt --all -- --check

for features in '--all-features' '--no-default-features' ; do
    cargo check --workspace --all-targets $features
    cargo clippy --workspace --all-targets $features -- -Dwarnings
    cargo test --workspace --all-targets $features
    cargo test --workspace --doc $features
done

cargo install --locked cargo-deny || true
cargo deny --workspace --all-features check --show-stats

cargo +1.77.0 check --workspace --all-targets --all-features

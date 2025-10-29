#!/usr/bin/env bash

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

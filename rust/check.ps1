#!/usr/bin/env pwsh

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

param(
    [string]$Toolchain = 'stable'
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Exit-IfFailed {
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

cargo "+$Toolchain" fmt --all -- --check
Exit-IfFailed

foreach ($buildProfile in @(
    @{ Name = "all-features"; Args = @("--all-features") },
    @{ Name = "alloc"; Args = @("--no-default-features", "--features", "alloc") },
    @{ Name = "core"; Args = @("--no-default-features") }
)) {
    cargo "+$Toolchain" check --workspace --all-targets @($buildProfile.Args)
    Exit-IfFailed

    cargo "+$Toolchain" clippy --workspace --all-targets @($buildProfile.Args) -- -Dwarnings
    Exit-IfFailed

    cargo "+$Toolchain" test --workspace --all-targets @($buildProfile.Args)
    Exit-IfFailed

    cargo "+$Toolchain" test --workspace --doc @($buildProfile.Args)
    Exit-IfFailed
}

rustup target add thumbv7em-none-eabi --toolchain $Toolchain
Exit-IfFailed

cargo "+$Toolchain" check --workspace --no-default-features --target thumbv7em-none-eabi
Exit-IfFailed

cargo "+$Toolchain" check --workspace --no-default-features --features alloc --target thumbv7em-none-eabi
Exit-IfFailed

cargo "+$Toolchain" install --locked cargo-deny
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Failed to install cargo-deny; continuing."
}

cargo "+$Toolchain" deny --workspace --all-features check --show-stats
Exit-IfFailed

cargo '+1.82.0' check --workspace --all-targets --all-features
Exit-IfFailed

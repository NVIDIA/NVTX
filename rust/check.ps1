#!/usr/bin/env pwsh

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

foreach ($features in @("--all-features", "--no-default-features")) {
    cargo "+$Toolchain" check --workspace --all-targets $features
    Exit-IfFailed

    cargo "+$Toolchain" clippy --workspace --all-targets $features -- -Dwarnings
    Exit-IfFailed

    cargo "+$Toolchain" test --workspace --all-targets $features
    Exit-IfFailed

    cargo "+$Toolchain" test --workspace --doc $features
    Exit-IfFailed
}

cargo "+$Toolchain" install --locked cargo-deny
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Failed to install cargo-deny; continuing."
}

cargo "+$Toolchain" deny --workspace --all-features check --show-stats
Exit-IfFailed

cargo '+1.82.0' check --workspace --all-targets --all-features
Exit-IfFailed

#!/usr/bin/env pwsh

param(
    [string]$Python = "python3"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Exit-IfFailed {
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& $Python -m pip install ".[test]"
Exit-IfFailed

& $Python -m pytest tests
Exit-IfFailed

& $Python -m pytest --enable-injection tests
Exit-IfFailed

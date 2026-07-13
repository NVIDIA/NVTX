// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

/// Error values returned by fallible NVTX API variants.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum NvtxError {
    /// A registered string was supplied where global-context APIs only support
    /// ASCII or Unicode messages.
    RegisteredStringInGlobalContext,
    /// A domain-owned value (for example category or registered string) was
    /// used with a different domain.
    DomainMismatch,
}

impl core::fmt::Display for NvtxError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::RegisteredStringInGlobalContext => {
                write!(f, "registered strings are not valid in the global context")
            }
            Self::DomainMismatch => write!(f, "domain-owned value used with a different domain"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for NvtxError {}

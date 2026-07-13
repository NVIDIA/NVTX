// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use alloc::{ffi::CString, string::String};
use core::ffi::CStr;
use widestring::error::ContainsNul;
use widestring::{WideCStr, WideCString};

/// A convenience wrapper for various string types.
///
/// * [`Str::Ascii`] is the discriminator for C string types
/// * [`Str::Unicode`] is the discriminator for Rust string types and C wide string types
#[derive(Debug, Clone)]
pub enum Str {
    /// Represents an ASCII friendly string
    Ascii(CString),
    /// Represents a Unicode string
    Unicode(WideCString),
}

/// Error returned when converting a Rust string into [`Str`] fails.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct StrError {
    nul_position: usize,
}

impl StrError {
    /// Return the position of the interior NUL that made conversion fail.
    #[must_use]
    pub fn nul_position(&self) -> usize {
        self.nul_position
    }
}

impl<C> From<ContainsNul<C>> for StrError {
    fn from(value: ContainsNul<C>) -> Self {
        Self {
            nul_position: value.nul_position(),
        }
    }
}

impl core::fmt::Display for StrError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "interior NUL found while converting string at position {}",
            self.nul_position
        )
    }
}

#[cfg(feature = "std")]
impl std::error::Error for StrError {}

impl Str {
    /// Convert an owned Rust string into [`Str`], rejecting interior NULs.
    ///
    /// # Errors
    ///
    /// Returns [`StrError`] when `value` contains an interior NUL.
    #[allow(clippy::needless_pass_by_value)]
    pub fn try_from_string(value: String) -> Result<Self, StrError> {
        Self::try_from_str(value.as_str())
    }

    /// Convert a borrowed Rust string into [`Str`], rejecting interior NULs.
    ///
    /// # Errors
    ///
    /// Returns [`StrError`] when `value` contains an interior NUL.
    pub fn try_from_str(value: &str) -> Result<Self, StrError> {
        WideCString::from_str(value)
            .map(Self::Unicode)
            .map_err(StrError::from)
    }

    /// Convert an owned Rust string into [`Str`], removing interior NULs.
    ///
    /// Use this constructor only when losing interior NULs is the intended
    /// behavior. Use [`Str::try_from_string`] when conversion must be lossless.
    #[must_use]
    pub fn from_string_lossy(mut value: String) -> Self {
        if let Ok(str_value) = Self::try_from_str(value.as_str()) {
            return str_value;
        }
        value.retain(|c| c != '\0');
        // SAFETY: The sanitized string was built by removing every NUL character.
        Self::Unicode(unsafe { WideCString::from_str_unchecked(value) })
    }

    /// Convert a borrowed Rust string into [`Str`], removing interior NULs.
    ///
    /// Use this constructor only when losing interior NULs is the intended
    /// behavior. Use [`Str::try_from_str`] when conversion must be lossless.
    #[must_use]
    pub fn from_str_lossy(value: &str) -> Self {
        if let Ok(str_value) = Self::try_from_str(value) {
            str_value
        } else {
            let sanitized: String = value.chars().filter(|c| *c != '\0').collect();
            // SAFETY: The sanitized string was built by removing every NUL character.
            Self::Unicode(unsafe { WideCString::from_str_unchecked(sanitized) })
        }
    }
}

impl TryFrom<String> for Str {
    type Error = StrError;

    fn try_from(value: String) -> Result<Self, Self::Error> {
        Self::try_from_string(value)
    }
}

impl TryFrom<&str> for Str {
    type Error = StrError;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        Self::try_from_str(value)
    }
}

impl From<CString> for Str {
    fn from(v: CString) -> Self {
        Self::Ascii(v)
    }
}

impl From<&CStr> for Str {
    fn from(v: &CStr) -> Self {
        CString::from(v).into()
    }
}

impl From<WideCString> for Str {
    fn from(v: WideCString) -> Self {
        Self::Unicode(v)
    }
}

impl From<&WideCStr> for Str {
    fn from(v: &WideCStr) -> Self {
        WideCString::from(v).into()
    }
}

#[cfg(test)]
mod tests {
    use super::{Str, StrError};
    use alloc::string::ToString;

    #[test]
    fn try_from_str_rejects_interior_nul() {
        let error = Str::try_from_str("abc\0def").unwrap_err();
        assert_eq!(error.nul_position(), 3);
        assert_eq!(
            error.to_string(),
            "interior NUL found while converting string at position 3"
        );
    }

    #[test]
    fn from_str_lossy_removes_interior_nuls() {
        let value = Str::from_str_lossy("abc\0def\0ghi");
        assert!(matches!(value, Str::Unicode(s) if s.to_string_lossy() == "abcdefghi"));
    }

    #[test]
    fn str_error_can_be_constructed_from_widestring_error() {
        let error: StrError = widestring::WideCString::from_str("a\0b")
            .unwrap_err()
            .into();
        assert_eq!(error.nul_position(), 1);
    }
}

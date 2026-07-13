// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::{domain::RegisteredString, NvtxError, Str, TypeValueEncodable};
use alloc::borrow::ToOwned;
use alloc::ffi::CString;
use alloc::string::String;
use core::ffi::CStr;
use widestring::{WideCStr, WideCString};

/// Generic message type that can be used in both global and domain contexts
#[derive(Debug, Clone)]
pub enum GenericMessage<T = ()> {
    /// An owned ASCII string.
    Ascii(CString),
    /// An owned Unicode string.
    Unicode(WideCString),
    /// A registered string handle (only available in domain context).
    Registered(T),
}

impl<T> From<Str> for GenericMessage<T> {
    fn from(value: Str) -> Self {
        match value {
            Str::Ascii(s) => Self::Ascii(s),
            Str::Unicode(s) => Self::Unicode(s),
        }
    }
}

impl<T> GenericMessage<T> {
    /// Convert an owned Rust string into a message, removing interior NULs.
    ///
    /// Use this constructor only when losing interior NULs is the intended
    /// behavior.
    #[must_use]
    pub fn from_string_lossy(value: String) -> Self {
        Self::from(Str::from_string_lossy(value))
    }

    /// Convert a borrowed Rust string into a message, removing interior NULs.
    ///
    /// Use this constructor only when losing interior NULs is the intended
    /// behavior.
    #[must_use]
    pub fn from_str_lossy(value: &str) -> Self {
        Self::from(Str::from_str_lossy(value))
    }
}

impl<'a, T> From<RegisteredString<'a>> for GenericMessage<T>
where
    T: From<RegisteredString<'a>>,
{
    fn from(v: RegisteredString<'a>) -> Self {
        Self::Registered(v.into())
    }
}

impl<T> From<CString> for GenericMessage<T> {
    fn from(value: CString) -> Self {
        Self::Ascii(value)
    }
}

impl<T> From<WideCString> for GenericMessage<T> {
    fn from(value: WideCString) -> Self {
        Self::Unicode(value)
    }
}

impl<T> From<&CStr> for GenericMessage<T> {
    fn from(value: &CStr) -> Self {
        Self::Ascii(value.to_owned())
    }
}

impl<T> From<&WideCStr> for GenericMessage<T> {
    fn from(value: &WideCStr) -> Self {
        Self::Unicode(value.to_owned())
    }
}

trait Encodable {
    fn encode(&self) -> (nvtx_sys::MessageType, nvtx_sys::MessageValue);
}

impl Encodable for () {
    fn encode(&self) -> (nvtx_sys::MessageType, nvtx_sys::MessageValue) {
        debug_assert!(false, "{}", NvtxError::RegisteredStringInGlobalContext);
        (
            nvtx_sys::MessageType::NVTX_MESSAGE_UNKNOWN,
            nvtx_sys::MessageValue {
                ascii: core::ptr::null(),
            },
        )
    }
}

impl Encodable for RegisteredString<'_> {
    fn encode(&self) -> (nvtx_sys::MessageType, nvtx_sys::MessageValue) {
        (
            nvtx_sys::MessageType::NVTX_MESSAGE_TYPE_REGISTERED,
            nvtx_sys::MessageValue {
                registered: self.handle().into(),
            },
        )
    }
}

impl<T> TypeValueEncodable for GenericMessage<T>
where
    T: Encodable,
{
    type Type = nvtx_sys::MessageType;
    type Value = nvtx_sys::MessageValue;

    fn encode(&self) -> (Self::Type, Self::Value) {
        match self {
            GenericMessage::Ascii(s) => (
                Self::Type::NVTX_MESSAGE_TYPE_ASCII,
                Self::Value { ascii: s.as_ptr() },
            ),
            GenericMessage::Unicode(s) => (
                Self::Type::NVTX_MESSAGE_TYPE_UNICODE,
                Self::Value {
                    unicode: s.as_ptr().cast(),
                },
            ),
            GenericMessage::Registered(r) => r.encode(),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        (
            Self::Type::NVTX_MESSAGE_UNKNOWN,
            Self::Value {
                ascii: core::ptr::null(),
            },
        )
    }
}

// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use super::Identifier;
use crate::TypeValueEncodable;

/// Identifiers used for CUDA resources
pub enum CudaRuntimeIdentifier {
    /// Device
    Device(i32),
    /// Event
    Event(nvtx_sys::CudaEvent),
    /// Stream
    Stream(nvtx_sys::CudaStream),
}

impl From<CudaRuntimeIdentifier> for Identifier {
    fn from(value: CudaRuntimeIdentifier) -> Self {
        Self::CudaRuntime(value)
    }
}

impl TypeValueEncodable for CudaRuntimeIdentifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        use nvtx_sys::resource_type::{CUDART_DEVICE, CUDART_EVENT, CUDART_STREAM};
        match self {
            Self::Device(id) => (
                CUDART_DEVICE,
                Self::Value {
                    // CAST: CUDA runtime device IDs are passed through as raw identifier bits for NVTX.
                    ullValue: *id as u64,
                },
            ),
            Self::Event(id) => (CUDART_EVENT, Self::Value { pValue: id.cast() }),
            Self::Stream(id) => (CUDART_STREAM, Self::Value { pValue: id.cast() }),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        Identifier::default_encoding()
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use std::os::raw::c_void;

    use super::*;

    #[test]
    fn test_identifier_device() {
        let device_id = 0;
        let x = CudaRuntimeIdentifier::Device(device_id);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::CudaRuntime(CudaRuntimeIdentifier::Device(id)) if id == device_id)
        );
    }

    #[test]
    fn test_identifier_event() {
        let ptr: nvtx_sys::CudaEvent = std::ptr::null_mut();
        let x = CudaRuntimeIdentifier::Event(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::CudaRuntime(CudaRuntimeIdentifier::Event(p)) if p == ptr));
    }

    #[test]
    fn test_identifier_stream() {
        let ptr: nvtx_sys::CudaStream = std::ptr::null_mut();
        let x = CudaRuntimeIdentifier::Stream(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::CudaRuntime(CudaRuntimeIdentifier::Stream(p)) if p == ptr));
    }

    #[test]
    fn test_encode_device() {
        let device_id = 0;
        let x = CudaRuntimeIdentifier::Device(device_id);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDART_DEVICE);
        // SAFETY: The device resource type is asserted above, so reading `ullValue` is valid.
        unsafe {
            // CAST: Test compares the encoded raw identifier bits exactly as NVTX stores them.
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { ullValue: id } if id == (device_id as u64))
            );
        }
    }

    #[test]
    fn test_encode_event() {
        let ptr: nvtx_sys::CudaEvent = std::ptr::null_mut();
        let x = CudaRuntimeIdentifier::Event(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDART_EVENT);
        // SAFETY: The event resource type is asserted above, so reading `pValue` is valid.
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr.cast_const().cast::<c_void>()))
            );
        }
    }

    #[test]
    fn test_encode_stream() {
        let ptr: nvtx_sys::CudaStream = std::ptr::null_mut();
        let x = CudaRuntimeIdentifier::Stream(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDART_STREAM);
        // SAFETY: The stream resource type is asserted above, so reading `pValue` is valid.
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr.cast_const().cast::<c_void>()))
            );
        }
    }
}

// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use super::Identifier;
use crate::TypeValueEncodable;

/// Identifiers used for `OpenCL` resources.
pub enum OpenClIdentifier {
    /// Device
    Device(nvtx_sys::ClDeviceId),
    /// Context
    Context(nvtx_sys::ClContext),
    /// Command queue
    CommandQueue(nvtx_sys::ClCommandQueue),
    /// Memory object
    MemObject(nvtx_sys::ClMem),
    /// Sampler
    Sampler(nvtx_sys::ClSampler),
    /// Program
    Program(nvtx_sys::ClProgram),
    /// Event
    Event(nvtx_sys::ClEvent),
}

impl From<OpenClIdentifier> for Identifier {
    fn from(value: OpenClIdentifier) -> Self {
        Self::OpenCl(value)
    }
}

impl TypeValueEncodable for OpenClIdentifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        use nvtx_sys::resource_type::{
            OPENCL_COMMANDQUEUE, OPENCL_CONTEXT, OPENCL_DEVICE, OPENCL_EVENT, OPENCL_MEMOBJECT,
            OPENCL_PROGRAM, OPENCL_SAMPLER,
        };

        match self {
            Self::Device(id) => (OPENCL_DEVICE, Self::Value { pValue: id.cast() }),
            Self::Context(id) => (OPENCL_CONTEXT, Self::Value { pValue: id.cast() }),
            Self::CommandQueue(id) => (OPENCL_COMMANDQUEUE, Self::Value { pValue: id.cast() }),
            Self::MemObject(id) => (OPENCL_MEMOBJECT, Self::Value { pValue: id.cast() }),
            Self::Sampler(id) => (OPENCL_SAMPLER, Self::Value { pValue: id.cast() }),
            Self::Program(id) => (OPENCL_PROGRAM, Self::Value { pValue: id.cast() }),
            Self::Event(id) => (OPENCL_EVENT, Self::Value { pValue: id.cast() }),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        Identifier::default_encoding()
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use core::ffi::c_void;

    use super::*;

    fn assert_pointer_encoding(
        identifier: &OpenClIdentifier,
        expected_type: u32,
        expected_pointer: *const c_void,
    ) {
        let (resource_type, value) = identifier.encode();
        assert_eq!(resource_type, expected_type);
        // SAFETY: Every OpenCL identifier is encoded in the union's `pValue` field.
        unsafe {
            assert!(core::ptr::eq(value.pValue, expected_pointer));
        }
    }

    #[test]
    fn test_identifier_conversion() {
        let device: nvtx_sys::ClDeviceId = core::ptr::null_mut();
        let context: nvtx_sys::ClContext = core::ptr::null_mut();
        let command_queue: nvtx_sys::ClCommandQueue = core::ptr::null_mut();
        let mem_object: nvtx_sys::ClMem = core::ptr::null_mut();
        let sampler: nvtx_sys::ClSampler = core::ptr::null_mut();
        let program: nvtx_sys::ClProgram = core::ptr::null_mut();
        let event: nvtx_sys::ClEvent = core::ptr::null_mut();

        assert!(matches!(
            Identifier::from(OpenClIdentifier::Device(device)),
            Identifier::OpenCl(OpenClIdentifier::Device(id)) if id == device
        ));
        assert!(matches!(
            Identifier::from(OpenClIdentifier::Context(context)),
            Identifier::OpenCl(OpenClIdentifier::Context(id)) if id == context
        ));
        assert!(matches!(
            Identifier::from(OpenClIdentifier::CommandQueue(command_queue)),
            Identifier::OpenCl(OpenClIdentifier::CommandQueue(id)) if id == command_queue
        ));
        assert!(matches!(
            Identifier::from(OpenClIdentifier::MemObject(mem_object)),
            Identifier::OpenCl(OpenClIdentifier::MemObject(id)) if id == mem_object
        ));
        assert!(matches!(
            Identifier::from(OpenClIdentifier::Sampler(sampler)),
            Identifier::OpenCl(OpenClIdentifier::Sampler(id)) if id == sampler
        ));
        assert!(matches!(
            Identifier::from(OpenClIdentifier::Program(program)),
            Identifier::OpenCl(OpenClIdentifier::Program(id)) if id == program
        ));
        assert!(matches!(
            Identifier::from(OpenClIdentifier::Event(event)),
            Identifier::OpenCl(OpenClIdentifier::Event(id)) if id == event
        ));
    }

    #[test]
    fn test_encode() {
        let device: nvtx_sys::ClDeviceId = core::ptr::null_mut();
        let context: nvtx_sys::ClContext = core::ptr::null_mut();
        let command_queue: nvtx_sys::ClCommandQueue = core::ptr::null_mut();
        let mem_object: nvtx_sys::ClMem = core::ptr::null_mut();
        let sampler: nvtx_sys::ClSampler = core::ptr::null_mut();
        let program: nvtx_sys::ClProgram = core::ptr::null_mut();
        let event: nvtx_sys::ClEvent = core::ptr::null_mut();

        assert_pointer_encoding(
            &OpenClIdentifier::Device(device),
            nvtx_sys::resource_type::OPENCL_DEVICE,
            device.cast_const().cast(),
        );
        assert_pointer_encoding(
            &OpenClIdentifier::Context(context),
            nvtx_sys::resource_type::OPENCL_CONTEXT,
            context.cast_const().cast(),
        );
        assert_pointer_encoding(
            &OpenClIdentifier::CommandQueue(command_queue),
            nvtx_sys::resource_type::OPENCL_COMMANDQUEUE,
            command_queue.cast_const().cast(),
        );
        assert_pointer_encoding(
            &OpenClIdentifier::MemObject(mem_object),
            nvtx_sys::resource_type::OPENCL_MEMOBJECT,
            mem_object.cast_const().cast(),
        );
        assert_pointer_encoding(
            &OpenClIdentifier::Sampler(sampler),
            nvtx_sys::resource_type::OPENCL_SAMPLER,
            sampler.cast_const().cast(),
        );
        assert_pointer_encoding(
            &OpenClIdentifier::Program(program),
            nvtx_sys::resource_type::OPENCL_PROGRAM,
            program.cast_const().cast(),
        );
        assert_pointer_encoding(
            &OpenClIdentifier::Event(event),
            nvtx_sys::resource_type::OPENCL_EVENT,
            event.cast_const().cast(),
        );
    }
}

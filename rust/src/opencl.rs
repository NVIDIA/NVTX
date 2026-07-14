// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::Str;

/// Enum for all `OpenCL` resource types.
pub enum OpenClResource {
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

impl From<nvtx_sys::ClDeviceId> for OpenClResource {
    fn from(value: nvtx_sys::ClDeviceId) -> Self {
        Self::Device(value)
    }
}

impl From<nvtx_sys::ClContext> for OpenClResource {
    fn from(value: nvtx_sys::ClContext) -> Self {
        Self::Context(value)
    }
}

impl From<nvtx_sys::ClCommandQueue> for OpenClResource {
    fn from(value: nvtx_sys::ClCommandQueue) -> Self {
        Self::CommandQueue(value)
    }
}

impl From<nvtx_sys::ClMem> for OpenClResource {
    fn from(value: nvtx_sys::ClMem) -> Self {
        Self::MemObject(value)
    }
}

impl From<nvtx_sys::ClSampler> for OpenClResource {
    fn from(value: nvtx_sys::ClSampler) -> Self {
        Self::Sampler(value)
    }
}

impl From<nvtx_sys::ClProgram> for OpenClResource {
    fn from(value: nvtx_sys::ClProgram) -> Self {
        Self::Program(value)
    }
}

impl From<nvtx_sys::ClEvent> for OpenClResource {
    fn from(value: nvtx_sys::ClEvent) -> Self {
        Self::Event(value)
    }
}

/// Name an `OpenCL` resource.
///
/// ```
/// let device = core::ptr::null_mut();
/// nvtx::name_opencl_resource(nvtx::OpenClResource::Device(device), c"GPU 0");
/// ```
pub fn name_opencl_resource(resource: impl Into<OpenClResource>, name: impl Into<Str>) {
    match resource.into() {
        OpenClResource::Device(device) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL device handle opaquely.
                unsafe { nvtx_sys::name_cl_device_ascii(device, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL device handle opaquely.
                unsafe { nvtx_sys::name_cl_device_unicode(device, &name) }
            }
        },
        OpenClResource::Context(context) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL context handle opaquely.
                unsafe { nvtx_sys::name_cl_context_ascii(context, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL context handle opaquely.
                unsafe { nvtx_sys::name_cl_context_unicode(context, &name) }
            }
        },
        OpenClResource::CommandQueue(command_queue) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL command queue handle opaquely.
                unsafe { nvtx_sys::name_cl_command_queue_ascii(command_queue, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL command queue handle opaquely.
                unsafe { nvtx_sys::name_cl_command_queue_unicode(command_queue, &name) }
            }
        },
        OpenClResource::MemObject(mem_object) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL memory object handle opaquely.
                unsafe { nvtx_sys::name_cl_mem_object_ascii(mem_object, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL memory object handle opaquely.
                unsafe { nvtx_sys::name_cl_mem_object_unicode(mem_object, &name) }
            }
        },
        OpenClResource::Sampler(sampler) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL sampler handle opaquely.
                unsafe { nvtx_sys::name_cl_sampler_ascii(sampler, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL sampler handle opaquely.
                unsafe { nvtx_sys::name_cl_sampler_unicode(sampler, &name) }
            }
        },
        OpenClResource::Program(program) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL program handle opaquely.
                unsafe { nvtx_sys::name_cl_program_ascii(program, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL program handle opaquely.
                unsafe { nvtx_sys::name_cl_program_unicode(program, &name) }
            }
        },
        OpenClResource::Event(event) => match name.into() {
            Str::Ascii(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL event handle opaquely.
                unsafe { nvtx_sys::name_cl_event_ascii(event, &name) }
            }
            Str::Unicode(name) => {
                // SAFETY: NVTX treats the caller-provided OpenCL event handle opaquely.
                unsafe { nvtx_sys::name_cl_event_unicode(event, &name) }
            }
        },
    }
}

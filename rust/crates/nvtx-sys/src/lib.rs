// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#![cfg_attr(test, allow(clippy::unwrap_used))]
#![no_std]
#![deny(unsafe_op_in_unsafe_fn)]

/// The unmodified FFI imported functions, types, and definitions
pub mod ffi {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

/// The NVTX version.
#[allow(clippy::cast_possible_truncation)]
// CAST: This constant cast stays `as` because `From`/`TryFrom` calls are not const on stable Rust.
pub const NVTX_VERSION: i16 = ffi::NVTX_VERSION as i16;

/// A unique range identifier.
pub type RangeId = ffi::nvtxRangeId_t;

/// Struct representing all possible Event attributes.
pub type EventAttributes = ffi::nvtxEventAttributes_t;

pub const NVTX_EVENT_ATTRIBUTES_SIZE: usize = core::mem::size_of::<EventAttributes>();

/// Struct representing all possible Resource attributes.
pub type ResourceAttributes = ffi::nvtxResourceAttributes_t;

pub const NVTX_RESOURCE_ATTRIBUTES_SIZE: usize = core::mem::size_of::<ResourceAttributes>();

/// Struct representing all possible User-defined Synchronization attributes.
pub type SyncUserAttributes = ffi::nvtxSyncUserAttributes_t;

pub const NVTX_SYNC_USER_ATTRIBUTES_SIZE: usize = core::mem::size_of::<SyncUserAttributes>();

/// [`ResourceAttributes`] identifier union.
pub type ResourceAttributesIdentifier = ffi::nvtxResourceAttributes_v0_identifier_t;
/// [`EventAttributes`] color type.
pub type ColorType = ffi::nvtxColorType_t;
/// [`EventAttributes`] and [`ResourceAttributes`] message type.
pub type MessageType = ffi::nvtxMessageType_t;
/// [`EventAttributes`] and [`ResourceAttributes`] message value.
pub type MessageValue = ffi::nvtxMessageValue_t;
/// [`EventAttributes`] payload type.
pub type PayloadType = ffi::nvtxPayloadType_t;
/// [`EventAttributes`] payload value union.
pub type PayloadValue = ffi::nvtxEventAttributes_v2_payload_t;

// Keep this declarative macro for tightly scoped impl boilerplate only.
macro_rules! impl_from_repr_u32_enum_for_i32 {
    ($($ty:ty),+ $(,)?) => {
        $(
            impl ::core::convert::From<$ty> for i32 {
                fn from(value: $ty) -> Self {
                    // CAST: Rust has no blanket enum->int `From`; `as` preserves the 32-bit bit pattern expected by NVTX's i32 fields.
                    value as i32
                }
            }
        )+
    };
}

impl_from_repr_u32_enum_for_i32!(ColorType, MessageType, PayloadType);

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_fits_nvtx_i32_field<T>(name: &str, value: T)
    where
        T: Into<i32>,
    {
        assert!(
            value.into() >= 0,
            "{name} must fit in NVTX's signed 32-bit type fields"
        );
    }

    #[test]
    fn enum_type_values_fit_nvtx_i32_fields() {
        // `nvToolsExt.h` defines these enum values as non-negative constants in
        // the range 0..=6 for NVTX's signed 32-bit type discriminator fields.
        assert_fits_nvtx_i32_field("NVTX_COLOR_UNKNOWN", ColorType::NVTX_COLOR_UNKNOWN);
        assert_fits_nvtx_i32_field("NVTX_COLOR_ARGB", ColorType::NVTX_COLOR_ARGB);

        assert_fits_nvtx_i32_field("NVTX_MESSAGE_UNKNOWN", MessageType::NVTX_MESSAGE_UNKNOWN);
        assert_fits_nvtx_i32_field(
            "NVTX_MESSAGE_TYPE_ASCII",
            MessageType::NVTX_MESSAGE_TYPE_ASCII,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_MESSAGE_TYPE_UNICODE",
            MessageType::NVTX_MESSAGE_TYPE_UNICODE,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_MESSAGE_TYPE_REGISTERED",
            MessageType::NVTX_MESSAGE_TYPE_REGISTERED,
        );

        assert_fits_nvtx_i32_field("NVTX_PAYLOAD_UNKNOWN", PayloadType::NVTX_PAYLOAD_UNKNOWN);
        assert_fits_nvtx_i32_field(
            "NVTX_PAYLOAD_TYPE_UNSIGNED_INT64",
            PayloadType::NVTX_PAYLOAD_TYPE_UNSIGNED_INT64,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_PAYLOAD_TYPE_INT64",
            PayloadType::NVTX_PAYLOAD_TYPE_INT64,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_PAYLOAD_TYPE_DOUBLE",
            PayloadType::NVTX_PAYLOAD_TYPE_DOUBLE,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_PAYLOAD_TYPE_UNSIGNED_INT32",
            PayloadType::NVTX_PAYLOAD_TYPE_UNSIGNED_INT32,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_PAYLOAD_TYPE_INT32",
            PayloadType::NVTX_PAYLOAD_TYPE_INT32,
        );
        assert_fits_nvtx_i32_field(
            "NVTX_PAYLOAD_TYPE_FLOAT",
            PayloadType::NVTX_PAYLOAD_TYPE_FLOAT,
        );
    }
}

/// Unique handle for a registered domain.
#[derive(Debug, Clone, Copy)]
pub struct DomainHandle {
    handle: ffi::nvtxDomainHandle_t,
}

// SAFETY: `DomainHandle` is an opaque FFI handle with no Rust aliasing guarantees of its own.
// Sending/sharing this value is equivalent to sending/sharing the raw C handle value.
unsafe impl Send for DomainHandle {}
// SAFETY: See the `Send` rationale above.
unsafe impl Sync for DomainHandle {}

/// Unique handle for a registered resource.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct ResourceHandle {
    handle: ffi::nvtxResourceHandle_t,
}

// SAFETY: `ResourceHandle` is an opaque FFI handle; thread-safety is defined by NVTX.
unsafe impl Send for ResourceHandle {}
// SAFETY: See the `Send` rationale above.
unsafe impl Sync for ResourceHandle {}

/// Unique handle for a registered string.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct StringHandle {
    handle: ffi::nvtxStringHandle_t,
}

impl From<StringHandle> for ffi::nvtxStringHandle_t {
    fn from(value: StringHandle) -> Self {
        value.handle
    }
}

// SAFETY: `StringHandle` is an immutable opaque handle managed by NVTX.
unsafe impl Send for StringHandle {}
// SAFETY: See the `Send` rationale above.
unsafe impl Sync for StringHandle {}

/// Unique handle for a registered user-defined synchronization object.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SyncUserHandle {
    handle: ffi::nvtxSyncUser_t,
}

// SAFETY: `SyncUserHandle` is an opaque NVTX token whose synchronization semantics are in NVTX.
unsafe impl Send for SyncUserHandle {}
// SAFETY: See the `Send` rationale above.
unsafe impl Sync for SyncUserHandle {}

#[cfg(feature = "cuda_runtime")]
/// An opaque CUDA Runtime event type.
pub type CudaEvent = ffi::cudaEvent_t;
#[cfg(feature = "cuda_runtime")]
/// An opaque CUDA Runtime stream type.
pub type CudaStream = ffi::cudaStream_t;

#[cfg(feature = "cuda")]
/// An opaque CUDA context type.
pub type CuContext = ffi::CUcontext;
#[cfg(feature = "cuda")]
/// An opaque CUDA device type.
pub type CuDevice = ffi::CUdevice;
#[cfg(feature = "cuda")]
/// An opaque CUDA event type.
pub type CuEvent = ffi::CUevent;
#[cfg(feature = "cuda")]
/// An opaque CUDA stream type.
pub type CuStream = ffi::CUstream;

/// Resource types for use within [`crate::ResourceAttributes`].
pub mod resource_type {
    #![allow(clippy::wildcard_imports)]
    #![allow(clippy::unnecessary_cast)]

    use crate::ffi::nvtxResourceGenericType_t::*;
    // CAST: Bindgen emits these enum constants as either `i32` or `u32` across platforms/targets.
    // Normalize to the raw 32-bit identifier representation expected by NVTX.
    /// An unknown resource type.
    pub const UNKNOWN: u32 = NVTX_RESOURCE_TYPE_UNKNOWN as u32;
    /// A handle to a generic resource.
    pub const GENERIC_HANDLE: u32 = NVTX_RESOURCE_TYPE_GENERIC_HANDLE as u32;
    /// A pointer to a generic resource.
    pub const GENERIC_POINTER: u32 = NVTX_RESOURCE_TYPE_GENERIC_POINTER as u32;
    /// A handle to a native thread.
    pub const GENERIC_THREAD_NATIVE: u32 = NVTX_RESOURCE_TYPE_GENERIC_THREAD_NATIVE as u32;
    /// A handle to a posix thread.
    pub const GENERIC_THREAD_POSIX: u32 = NVTX_RESOURCE_TYPE_GENERIC_THREAD_POSIX as u32;

    #[cfg(feature = "cuda")]
    mod cuda {
        use crate::ffi::nvtxResourceCUDAType_t::*;
        // CAST: See `resource_type::UNKNOWN` for cast rationale.
        /// A CUDA device resource.
        pub const CUDA_DEVICE: u32 = NVTX_RESOURCE_TYPE_CUDA_DEVICE as u32;
        /// A CUDA context resource.
        pub const CUDA_CONTEXT: u32 = NVTX_RESOURCE_TYPE_CUDA_CONTEXT as u32;
        /// A CUDA stream resource.
        pub const CUDA_STREAM: u32 = NVTX_RESOURCE_TYPE_CUDA_STREAM as u32;
        /// A CUDA event resource.
        pub const CUDA_EVENT: u32 = NVTX_RESOURCE_TYPE_CUDA_EVENT as u32;
    }
    #[cfg(feature = "cuda")]
    pub use cuda::*;

    #[cfg(feature = "cuda_runtime")]
    mod cuda_runtime {
        use crate::ffi::nvtxResourceCUDARTType_t::*;
        // CAST: See `resource_type::UNKNOWN` for cast rationale.
        /// A CUDA runtime device resource.
        pub const CUDART_DEVICE: u32 = NVTX_RESOURCE_TYPE_CUDART_DEVICE as u32;
        /// A CUDA runtime stream resource.
        pub const CUDART_STREAM: u32 = NVTX_RESOURCE_TYPE_CUDART_STREAM as u32;
        /// A CUDA runtime event resource.
        pub const CUDART_EVENT: u32 = NVTX_RESOURCE_TYPE_CUDART_EVENT as u32;
    }
    #[cfg(feature = "cuda_runtime")]
    pub use cuda_runtime::*;

    #[cfg(target_family = "unix")]
    mod pthread {
        use crate::ffi::nvtxResourceSyncPosixThreadType_t::*;
        // CAST: See `resource_type::UNKNOWN` for cast rationale.
        /// A pthread mutex resource.
        pub const PTHREAD_MUTEX: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_MUTEX as u32;
        /// A pthread condition variable resource.
        pub const PTHREAD_CONDITION: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_CONDITION as u32;
        /// A pthread rwlock resource.
        pub const PTHREAD_RWLOCK: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_RWLOCK as u32;
        /// A pthread barrier resource.
        pub const PTHREAD_BARRIER: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_BARRIER as u32;
        /// A pthread spinlock resource.
        pub const PTHREAD_SPINLOCK: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_SPINLOCK as u32;
        /// A pthread oncelock resource.
        pub const PTHREAD_ONCE: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_ONCE as u32;
    }
    #[cfg(target_family = "unix")]
    pub use pthread::*;
}

use core::ffi::CStr;
use widestring::WideCStr;

/// Create a mark within a domain.
pub fn domain_mark_ex(domain: DomainHandle, event_attrib: &EventAttributes) {
    // SAFETY: We forward a valid NVTX domain handle plus an immutable attributes pointer.
    unsafe { crate::ffi::nvtxDomainMarkEx(domain.handle, event_attrib) }
}

/// Create a mark with an attributes structure.
pub fn mark_ex(event_attrib: &EventAttributes) {
    // SAFETY: We pass an immutable attributes pointer directly to NVTX.
    unsafe { crate::ffi::nvtxMarkEx(event_attrib) }
}

/// Create a mark with an ASCII string.
pub fn mark_ascii(message: &CStr) {
    // SAFETY: `message` is a valid NUL-terminated C string for the call duration.
    unsafe { crate::ffi::nvtxMarkA(message.as_ptr()) }
}

/// Create a mark with a Unicode string.
pub fn mark_unicode(message: &WideCStr) {
    // SAFETY: `message` points to a valid wide C string for the call duration.
    unsafe { crate::ffi::nvtxMarkW(message.as_ptr().cast()) }
}

#[must_use]
/// Start a process-visible range within a domain with an attributes structure.
///
/// To close the range, see [`domain_range_end`].
pub fn domain_range_start_ex(domain: DomainHandle, event_attrib: &EventAttributes) -> RangeId {
    // SAFETY: We forward a valid NVTX domain handle plus an immutable attributes pointer.
    unsafe { crate::ffi::nvtxDomainRangeStartEx(domain.handle, event_attrib) }
}

#[must_use]
/// Start a process-visible range with an attributes structure.
///
/// To close the range, see [`range_end`].
pub fn range_start_ex(event_attrib: &EventAttributes) -> RangeId {
    // SAFETY: We pass an immutable attributes pointer directly to NVTX.
    unsafe { crate::ffi::nvtxRangeStartEx(event_attrib) }
}

#[must_use]
/// Start a process-visible range with an ASCII string.
///
/// To close the range, see [`range_end`].
pub fn range_start_ascii(message: &CStr) -> RangeId {
    // SAFETY: `message` is a valid NUL-terminated C string for the call duration.
    unsafe { crate::ffi::nvtxRangeStartA(message.as_ptr()) }
}

#[must_use]
/// Start a process-visible range with a Unicode string.
///
/// To close the range, see [`range_end`].
pub fn range_start_unicode(message: &WideCStr) -> RangeId {
    // SAFETY: `message` points to a valid wide C string for the call duration.
    unsafe { crate::ffi::nvtxRangeStartW(message.as_ptr().cast()) }
}

/// End a process-visible range within a domain.
///
/// The range id is created by [`domain_range_start_ex`]
pub fn domain_range_end(domain: DomainHandle, id: RangeId) {
    // SAFETY: `domain` and `id` originate from NVTX range start APIs.
    unsafe { crate::ffi::nvtxDomainRangeEnd(domain.handle, id) }
}

/// End a process-visible range.
///
/// The range id is created by one of:
/// * [`range_start_ascii`]
/// * [`range_start_unicode`]
/// * [`range_start_ex`]
pub fn range_end(id: RangeId) {
    // SAFETY: `id` originates from an NVTX range start API.
    unsafe { crate::ffi::nvtxRangeEnd(id) }
}

#[allow(clippy::must_use_candidate)]
/// Start a thread-visible range within a domain with an attributes structure.
///
/// To close, see [`domain_range_pop`].
pub fn domain_range_push_ex(domain: DomainHandle, event_attrib: &EventAttributes) -> i32 {
    // SAFETY: We forward a valid NVTX domain handle plus an immutable attributes pointer.
    unsafe { crate::ffi::nvtxDomainRangePushEx(domain.handle, event_attrib) }
}

#[allow(clippy::must_use_candidate)]
/// Start a thread-visible range with an attributes structure.
///
/// To close, see [`range_pop`].
pub fn range_push_ex(event_attrib: &EventAttributes) -> i32 {
    // SAFETY: We pass an immutable attributes pointer directly to NVTX.
    unsafe { crate::ffi::nvtxRangePushEx(event_attrib) }
}

#[allow(clippy::must_use_candidate)]
/// Start a thread-visible range with an ASCII string.
///
/// To close, see [`range_pop`].
pub fn range_push_ascii(message: &CStr) -> i32 {
    // SAFETY: `message` is a valid NUL-terminated C string for the call duration.
    unsafe { crate::ffi::nvtxRangePushA(message.as_ptr()) }
}

#[allow(clippy::must_use_candidate)]
/// Start a thread-visible range with a Unicode string.
///
/// To close, see [`range_pop`].
pub fn range_push_unicode(message: &WideCStr) -> i32 {
    // SAFETY: `message` points to a valid wide C string for the call duration.
    unsafe { crate::ffi::nvtxRangePushW(message.as_ptr().cast()) }
}

#[allow(clippy::must_use_candidate)]
/// End a thread-visible range within a domain.
///
/// The range would have been created via [`domain_range_push_ex`].
pub fn domain_range_pop(domain: DomainHandle) -> i32 {
    // SAFETY: `domain` is an opaque handle created by NVTX.
    unsafe { crate::ffi::nvtxDomainRangePop(domain.handle) }
}

#[allow(clippy::must_use_candidate)]
/// End a thread-visible range.
///
/// The range would have been created via one of:
/// * [`range_push_ascii`]
/// * [`range_push_unicode`]
/// * [`range_push_ex`]
pub fn range_pop() -> i32 {
    // SAFETY: Pops the current thread-local NVTX range stack in this process.
    unsafe { crate::ffi::nvtxRangePop() }
}

#[must_use]
/// Create a named resource within a domain.
///
/// To destroy the resource, see [`domain_resource_destroy`].
pub fn domain_resource_create(domain: DomainHandle, attribs: ResourceAttributes) -> ResourceHandle {
    ResourceHandle {
        // SAFETY: `domain` is an opaque NVTX handle and `attribs` lives across the call.
        handle: unsafe {
            crate::ffi::nvtxDomainResourceCreate(
                domain.handle,
                core::ptr::addr_of!(attribs).cast_mut(),
            )
        },
    }
}

/// Destroy a named resource.
///
/// The named resource is created by [`domain_resource_create`].
pub fn domain_resource_destroy(resource: ResourceHandle) {
    // SAFETY: `resource` was created by NVTX and is consumed by this destroy call.
    unsafe { crate::ffi::nvtxDomainResourceDestroy(resource.handle) }
}

/// Name a category within a domain with an ASCII string.
pub fn domain_name_category_ascii(domain: DomainHandle, category: u32, name: &CStr) {
    // SAFETY: `domain` is valid and `name` is a valid C string.
    unsafe { crate::ffi::nvtxDomainNameCategoryA(domain.handle, category, name.as_ptr()) }
}

/// Name a category within a domain with a Unicode string.
pub fn domain_name_category_unicode(domain: DomainHandle, category: u32, name: &WideCStr) {
    // SAFETY: `domain` is valid and `name` is a valid wide C string.
    unsafe { crate::ffi::nvtxDomainNameCategoryW(domain.handle, category, name.as_ptr().cast()) }
}

/// Name a category with an ASCII string.
pub fn name_category_ascii(category: u32, name: &CStr) {
    // SAFETY: `name` is a valid C string.
    unsafe { crate::ffi::nvtxNameCategoryA(category, name.as_ptr()) }
}

/// Name a category with a Unicode string.
pub fn name_category_unicode(category: u32, name: &WideCStr) {
    // SAFETY: `name` is a valid wide C string.
    unsafe { crate::ffi::nvtxNameCategoryW(category, name.as_ptr().cast()) }
}

/// Name an OS thread with an ASCII string.
///
/// Note: the threadId must be an operating-specific thread id. On Linux this would be a process's tid.
pub fn name_os_thread_ascii(thread_id: u32, name: &CStr) {
    // SAFETY: `name` is a valid C string and `thread_id` is passed through verbatim.
    unsafe { crate::ffi::nvtxNameOsThreadA(thread_id, name.as_ptr()) }
}

/// Name an OS thread with a Unicode string.
///
/// Note: the threadId must be an operating-specific thread id. On Linux this would be a process's tid.
pub fn name_os_thread_unicode(thread_id: u32, name: &WideCStr) {
    // SAFETY: `name` is a valid wide C string and `thread_id` is passed through verbatim.
    unsafe { crate::ffi::nvtxNameOsThreadW(thread_id, name.as_ptr().cast()) }
}

#[must_use]
/// Register an immutable ASCII string with a domain.
pub fn domain_register_string_ascii(domain: DomainHandle, string: &CStr) -> StringHandle {
    StringHandle {
        // SAFETY: `domain` is valid and `string` is a valid C string for the call.
        handle: unsafe { crate::ffi::nvtxDomainRegisterStringA(domain.handle, string.as_ptr()) },
    }
}

#[must_use]
/// Register an immutable Unicode string with a domain.
pub fn domain_register_string_unicode(domain: DomainHandle, string: &WideCStr) -> StringHandle {
    StringHandle {
        // SAFETY: `domain` is valid and `string` is a valid wide C string for the call.
        handle: unsafe {
            crate::ffi::nvtxDomainRegisterStringW(domain.handle, string.as_ptr().cast())
        },
    }
}

#[must_use]
/// Create a new domain with a given ASCII string name.
pub fn domain_create_ascii(name: &CStr) -> DomainHandle {
    DomainHandle {
        // SAFETY: `name` is a valid C string for the call duration.
        handle: unsafe { crate::ffi::nvtxDomainCreateA(name.as_ptr()) },
    }
}

#[must_use]
/// Create a new domain with a given Unicode string name.
pub fn domain_create_unicode(name: &WideCStr) -> DomainHandle {
    DomainHandle {
        // SAFETY: `name` is a valid wide C string for the call duration.
        handle: unsafe { crate::ffi::nvtxDomainCreateW(name.as_ptr().cast()) },
    }
}

/// Destroy a domain.
///
/// The domain is created by [`domain_create_ascii`] or [`domain_create_unicode`].
pub fn domain_destroy(domain: DomainHandle) {
    // SAFETY: `domain` is an opaque handle created by NVTX.
    unsafe { crate::ffi::nvtxDomainDestroy(domain.handle) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA device with an ASCII string.
pub fn name_cudevice_ascii(device: CuDevice, name: &CStr) {
    // SAFETY: `device` is forwarded opaquely and `name` is a valid C string.
    unsafe { crate::ffi::nvtxNameCuDeviceA(device, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA device with a Unicode string.
pub fn name_cudevice_unicode(device: CuDevice, name: &WideCStr) {
    // SAFETY: `device` is forwarded opaquely and `name` is a valid wide C string.
    unsafe { crate::ffi::nvtxNameCuDeviceW(device, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA context with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA context.
pub unsafe fn name_cucontext_ascii(context: CuContext, name: &CStr) {
    // SAFETY: Caller guarantees `context` is a valid CUDA context handle.
    unsafe { crate::ffi::nvtxNameCuContextA(context, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA context with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA context.
pub unsafe fn name_cucontext_unicode(context: CuContext, name: &WideCStr) {
    // SAFETY: Caller guarantees `context` is a valid CUDA context handle.
    unsafe { crate::ffi::nvtxNameCuContextW(context, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA stream with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_custream_ascii(stream: CuStream, name: &CStr) {
    // SAFETY: Caller guarantees `stream` is a valid CUDA stream handle.
    unsafe { crate::ffi::nvtxNameCuStreamA(stream, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA stream with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_custream_unicode(stream: CuStream, name: &WideCStr) {
    // SAFETY: Caller guarantees `stream` is a valid CUDA stream handle.
    unsafe { crate::ffi::nvtxNameCuStreamW(stream, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA event with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuevent_ascii(event: CuEvent, name: &CStr) {
    // SAFETY: Caller guarantees `event` is a valid CUDA event handle.
    unsafe { crate::ffi::nvtxNameCuEventA(event, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA event with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuevent_unicode(event: CuEvent, name: &WideCStr) {
    // SAFETY: Caller guarantees `event` is a valid CUDA event handle.
    unsafe { crate::ffi::nvtxNameCuEventW(event, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime device with an ASCII string.
pub fn name_cuda_device_ascii(device: i32, name: &CStr) {
    // SAFETY: `device` is passed through as the CUDA runtime device ID and `name` is valid.
    unsafe { crate::ffi::nvtxNameCudaDeviceA(device, name.as_ptr()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime device with a Unicode string.
pub fn name_cuda_device_unicode(device: i32, name: &WideCStr) {
    // SAFETY: `device` is passed through as the CUDA runtime device ID and `name` is valid.
    unsafe { crate::ffi::nvtxNameCudaDeviceW(device, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime stream with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_cuda_stream_ascii(stream: CudaStream, name: &CStr) {
    // SAFETY: Caller guarantees `stream` is a valid CUDA runtime stream handle.
    unsafe { crate::ffi::nvtxNameCudaStreamA(stream, name.as_ptr()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime stream with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_cuda_stream_unicode(stream: CudaStream, name: &WideCStr) {
    // SAFETY: Caller guarantees `stream` is a valid CUDA runtime stream handle.
    unsafe { crate::ffi::nvtxNameCudaStreamW(stream, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime event with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuda_event_ascii(event: CudaEvent, name: &CStr) {
    // SAFETY: Caller guarantees `event` is a valid CUDA runtime event handle.
    unsafe { crate::ffi::nvtxNameCudaEventA(event, name.as_ptr()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime event with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuda_event_unicode(event: CudaEvent, name: &WideCStr) {
    // SAFETY: Caller guarantees `event` is a valid CUDA runtime event handle.
    unsafe { crate::ffi::nvtxNameCudaEventW(event, name.as_ptr().cast()) }
}

#[must_use]
/// Create a new user-defined synchronization within a domain.
pub fn domain_syncuser_create(domain: DomainHandle, attribs: SyncUserAttributes) -> SyncUserHandle {
    SyncUserHandle {
        // SAFETY: `domain` is valid and `attribs` lives across the FFI call.
        handle: unsafe {
            crate::ffi::nvtxDomainSyncUserCreate(
                domain.handle,
                core::ptr::addr_of!(attribs).cast_mut(),
            )
        },
    }
}

/// Destroy a user-defined synchronization.
///
/// Created by [`domain_syncuser_create`].
pub fn domain_syncuser_destroy(handle: SyncUserHandle) {
    // SAFETY: `handle` was created by NVTX and is consumed by this destroy call.
    unsafe { crate::ffi::nvtxDomainSyncUserDestroy(handle.handle) }
}

/// Indicate that a user-defined synchronization started to acquire.
pub fn domain_syncuser_acquire_start(handle: SyncUserHandle) {
    // SAFETY: `handle` is an opaque sync-user token created by NVTX.
    unsafe { crate::ffi::nvtxDomainSyncUserAcquireStart(handle.handle) }
}

/// Indicate that a user-defined synchronization acquisition failed.
///
/// Note: this call is only valid after a call to [`domain_syncuser_acquire_start`].
pub fn domain_syncuser_acquire_failed(handle: SyncUserHandle) {
    // SAFETY: `handle` is an opaque sync-user token created by NVTX.
    unsafe { crate::ffi::nvtxDomainSyncUserAcquireFailed(handle.handle) }
}

/// Indicate that a user-defined synchronization acquisition succeeded.
///
/// Note: this call is only valid after a call to [`domain_syncuser_acquire_start`].
pub fn domain_syncuser_acquire_success(handle: SyncUserHandle) {
    // SAFETY: `handle` is an opaque sync-user token created by NVTX.
    unsafe { crate::ffi::nvtxDomainSyncUserAcquireSuccess(handle.handle) }
}

/// Indicate that a user-defined synchronization is released.
///
/// Note: this call is only valid after a call to [`domain_syncuser_acquire_success`].
pub fn domain_syncuser_acquire_releasing(handle: SyncUserHandle) {
    // SAFETY: `handle` is an opaque sync-user token created by NVTX.
    unsafe { crate::ffi::nvtxDomainSyncUserReleasing(handle.handle) }
}

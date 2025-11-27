#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

/// The unmodified FFI imported functions, types, and definitions
pub mod ffi {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

/// The NVTX version.
pub const NVTX_VERSION: i16 = ffi::NVTX_VERSION as i16;

/// A unique range identifier.
pub type RangeId = ffi::nvtxRangeId_t;

/// Struct representing all possible Event attributes.
pub type EventAttributes = ffi::nvtxEventAttributes_t;

pub const NVTX_EVENT_ATTRIBUTES_SIZE: usize = ::std::mem::size_of::<EventAttributes>();

/// Struct representing all possible Resource attributes.
pub type ResourceAttributes = ffi::nvtxResourceAttributes_t;

/// Struct representing all possible User-defined Synchronization attributes.
pub type SyncUserAttributes = ffi::nvtxSyncUserAttributes_t;

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

/// Unique handle for a registered domain.
#[derive(Debug, Clone, Copy)]
pub struct DomainHandle {
    handle: ffi::nvtxDomainHandle_t,
}

unsafe impl Send for DomainHandle {}
unsafe impl Sync for DomainHandle {}

/// Unique handle for a registered resource.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct ResourceHandle {
    handle: ffi::nvtxResourceHandle_t,
}

unsafe impl Send for ResourceHandle {}
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

unsafe impl Send for StringHandle {}
unsafe impl Sync for StringHandle {}

/// Unique handle for a registered user-defined synchronization object.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SyncUserHandle {
    handle: ffi::nvtxSyncUser_t,
}

unsafe impl Send for SyncUserHandle {}
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
    use crate::ffi::nvtxResourceGenericType_t::*;
    /// An unknown resource type.
    pub const UNKNOWN: u32 = NVTX_RESOURCE_TYPE_UNKNOWN;
    /// A handle to a generic resource.
    pub const GENERIC_HANDLE: u32 = NVTX_RESOURCE_TYPE_GENERIC_HANDLE;
    /// A pointer to a generic resource.
    pub const GENERIC_POINTER: u32 = NVTX_RESOURCE_TYPE_GENERIC_POINTER;
    /// A handle to a native thread.
    pub const GENERIC_THREAD_NATIVE: u32 = NVTX_RESOURCE_TYPE_GENERIC_THREAD_NATIVE;
    /// A handle to a posix thread.
    pub const GENERIC_THREAD_POSIX: u32 = NVTX_RESOURCE_TYPE_GENERIC_THREAD_POSIX;

    #[cfg(feature = "cuda")]
    mod cuda {
        use crate::ffi::nvtxResourceCUDAType_t::*;
        /// A CUDA device resource.
        pub const CUDA_DEVICE: u32 = NVTX_RESOURCE_TYPE_CUDA_DEVICE;
        /// A CUDA context resource.
        pub const CUDA_CONTEXT: u32 = NVTX_RESOURCE_TYPE_CUDA_CONTEXT;
        /// A CUDA stream resource.
        pub const CUDA_STREAM: u32 = NVTX_RESOURCE_TYPE_CUDA_STREAM;
        /// A CUDA event resource.
        pub const CUDA_EVENT: u32 = NVTX_RESOURCE_TYPE_CUDA_EVENT;
    }
    #[cfg(feature = "cuda")]
    pub use cuda::*;

    #[cfg(feature = "cuda_runtime")]
    mod cuda_runtime {
        use crate::ffi::nvtxResourceCUDARTType_t::*;
        /// A CUDA runtime device resource.
        pub const CUDART_DEVICE: u32 = NVTX_RESOURCE_TYPE_CUDART_DEVICE;
        /// A CUDA runtime stream resource.
        pub const CUDART_STREAM: u32 = NVTX_RESOURCE_TYPE_CUDART_STREAM;
        /// A CUDA runtime event resource.
        pub const CUDART_EVENT: u32 = NVTX_RESOURCE_TYPE_CUDART_EVENT;
    }
    #[cfg(feature = "cuda_runtime")]
    pub use cuda_runtime::*;

    #[cfg(target_family = "unix")]
    mod pthread {
        use crate::ffi::nvtxResourceSyncPosixThreadType_t::*;
        /// A pthread mutex resource.
        pub const PTHREAD_MUTEX: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_MUTEX;
        /// A pthread condition variable resource.
        pub const PTHREAD_CONDITION: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_CONDITION;
        /// A pthread rwlock resource.
        pub const PTHREAD_RWLOCK: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_RWLOCK;
        /// A pthread barrier resource.
        pub const PTHREAD_BARRIER: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_BARRIER;
        /// A pthread spinlock resource.
        pub const PTHREAD_SPINLOCK: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_SPINLOCK;
        /// A pthread oncelock resource.
        pub const PTHREAD_ONCE: u32 = NVTX_RESOURCE_TYPE_SYNC_PTHREAD_ONCE;
    }
    #[cfg(target_family = "unix")]
    pub use pthread::*;
}

use std::ffi::CStr;
use widestring::WideCStr;

/// Create a mark within a domain.
pub fn domain_mark_ex(domain: DomainHandle, eventAttrib: &EventAttributes) {
    unsafe { crate::ffi::nvtxDomainMarkEx(domain.handle, eventAttrib) }
}

/// Create a mark with an attributes structure.
pub fn mark_ex(eventAttrib: &EventAttributes) {
    unsafe { crate::ffi::nvtxMarkEx(eventAttrib) }
}

/// Create a mark with an ASCII string.
pub fn mark_ascii(message: &CStr) {
    unsafe { crate::ffi::nvtxMarkA(message.as_ptr()) }
}

/// Create a mark with a Unicode string.
pub fn mark_unicode(message: &WideCStr) {
    unsafe { crate::ffi::nvtxMarkW(message.as_ptr().cast()) }
}

#[must_use]
/// Start a process-visible range within a domain with an attributes structure.
///
/// To close the range, see [`domain_range_end`].
pub fn domain_range_start_ex(domain: DomainHandle, eventAttrib: &EventAttributes) -> RangeId {
    unsafe { crate::ffi::nvtxDomainRangeStartEx(domain.handle, eventAttrib) }
}

#[must_use]
/// Start a process-visible range with an attributes structure.
///
/// To close the range, see [`range_end`].
pub fn range_start_ex(eventAttrib: &EventAttributes) -> RangeId {
    unsafe { crate::ffi::nvtxRangeStartEx(eventAttrib) }
}

#[must_use]
/// Start a process-visible range with an ASCII string.
///
/// To close the range, see [`range_end`].
pub fn range_start_ascii(message: &CStr) -> RangeId {
    unsafe { crate::ffi::nvtxRangeStartA(message.as_ptr()) }
}

#[must_use]
/// Start a process-visible range with a Unicde string.
///
/// To close the range, see [`range_end`].
pub fn range_start_unicode(message: &WideCStr) -> RangeId {
    unsafe { crate::ffi::nvtxRangeStartW(message.as_ptr().cast()) }
}

/// End a process-visible range within a domain.
///
/// The range id is created by [`domain_range_start_ex`]
pub fn domain_range_end(domain: DomainHandle, id: RangeId) {
    unsafe { crate::ffi::nvtxDomainRangeEnd(domain.handle, id) }
}

/// End a process-visible range.
///
/// The range id is created by one of:
/// * [`range_start_ascii`]
/// * [`range_start_unicode`]
/// * [`range_start_ex`]
pub fn range_end(id: RangeId) {
    unsafe { crate::ffi::nvtxRangeEnd(id) }
}

/// Start a thread-visible range within a domain with an attributes structure.
///
/// To close, see [`domain_range_pop`].
pub fn domain_range_push_ex(domain: DomainHandle, eventAttrib: &EventAttributes) -> i32 {
    unsafe { crate::ffi::nvtxDomainRangePushEx(domain.handle, eventAttrib) }
}

/// Start a thread-visible range with an attributes structure.
///
/// To close, see [`range_pop`].
pub fn range_push_ex(eventAttrib: &EventAttributes) -> i32 {
    unsafe { crate::ffi::nvtxRangePushEx(eventAttrib) }
}

/// Start a thread-visible range with an ASCII string.
///
/// To close, see [`range_pop`].
pub fn range_push_ascii(message: &CStr) -> i32 {
    unsafe { crate::ffi::nvtxRangePushA(message.as_ptr()) }
}

/// Start a thread-visible range with a Unicode string.
///
/// To close, see [`range_pop`].
pub fn range_push_unicode(message: &WideCStr) -> i32 {
    unsafe { crate::ffi::nvtxRangePushW(message.as_ptr().cast()) }
}

/// End a thread-visible range within a domain.
///
/// The range would have been created via [`domain_range_push_ex`].
pub fn domain_range_pop(domain: DomainHandle) -> i32 {
    unsafe { crate::ffi::nvtxDomainRangePop(domain.handle) }
}

/// End a thread-visible range.
///
/// The range would have been created via one of:
/// * [`range_push_ascii`]
/// * [`range_push_unicode`]
/// * [`range_push_ex`]
pub fn range_pop() -> i32 {
    unsafe { crate::ffi::nvtxRangePop() }
}

#[must_use]
/// Create a named resource within a domain.
///
/// To destroy the resource, see [`domain_resource_destroy`].
pub fn domain_resource_create(domain: DomainHandle, attribs: ResourceAttributes) -> ResourceHandle {
    ResourceHandle {
        handle: unsafe {
            crate::ffi::nvtxDomainResourceCreate(
                domain.handle,
                std::ptr::addr_of!(attribs).cast_mut(),
            )
        },
    }
}

/// Destroy a named resource.
///
/// The named resource is created by [`domain_resource_create`].
pub fn domain_resource_destroy(resource: ResourceHandle) {
    unsafe { crate::ffi::nvtxDomainResourceDestroy(resource.handle) }
}

/// Name a category within a domain with an ASCII string.
pub fn domain_name_category_ascii(domain: DomainHandle, category: u32, name: &CStr) {
    unsafe { crate::ffi::nvtxDomainNameCategoryA(domain.handle, category, name.as_ptr()) }
}

/// Name a category within a domain with a Unicode string.
pub fn domain_name_category_unicode(domain: DomainHandle, category: u32, name: &WideCStr) {
    unsafe { crate::ffi::nvtxDomainNameCategoryW(domain.handle, category, name.as_ptr().cast()) }
}

/// Name a category with an ASCII string.
pub fn name_category_ascii(category: u32, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCategoryA(category, name.as_ptr()) }
}

/// Name a category with a Unicode string.
pub fn name_category_unicode(category: u32, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCategoryW(category, name.as_ptr().cast()) }
}

/// Name an OS thread with an ASCII string.
///
/// Note: the threadId must be an operating-specific thread id. On Linux this would be a process's tid.
pub fn name_os_thread_ascii(threadId: u32, name: &CStr) {
    unsafe { crate::ffi::nvtxNameOsThreadA(threadId, name.as_ptr()) }
}

/// Name an OS thread with a Unicode string.
///
/// Note: the threadId must be an operating-specific thread id. On Linux this would be a process's tid.
pub fn name_os_thread_unicode(threadId: u32, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameOsThreadW(threadId, name.as_ptr().cast()) }
}

#[must_use]
/// Register an immutable ASCII string with a domain.
pub fn domain_register_string_ascii(domain: DomainHandle, string: &CStr) -> StringHandle {
    StringHandle {
        handle: unsafe { crate::ffi::nvtxDomainRegisterStringA(domain.handle, string.as_ptr()) },
    }
}

#[must_use]
/// Register an immutable Unicode string with a domain.
pub fn domain_register_string_unicode(domain: DomainHandle, string: &WideCStr) -> StringHandle {
    StringHandle {
        handle: unsafe {
            crate::ffi::nvtxDomainRegisterStringW(domain.handle, string.as_ptr().cast())
        },
    }
}

#[must_use]
/// Create a new domain with a given ASCII string name.
pub fn domain_create_ascii(name: &CStr) -> DomainHandle {
    DomainHandle {
        handle: unsafe { crate::ffi::nvtxDomainCreateA(name.as_ptr()) },
    }
}

#[must_use]
/// Create a new domain with a given Unicode string name.
pub fn domain_create_unicode(name: &WideCStr) -> DomainHandle {
    DomainHandle {
        handle: unsafe { crate::ffi::nvtxDomainCreateW(name.as_ptr().cast()) },
    }
}

/// Destroy a domain.
///
/// The domain is created by [`domain_create_ascii`] or [`domain_create_unicode`].
pub fn domain_destroy(domain: DomainHandle) {
    unsafe { crate::ffi::nvtxDomainDestroy(domain.handle) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA device with an ASCII string.
pub fn name_cudevice_ascii(device: CuDevice, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCuDeviceA(device, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA device with a Unicode string.
pub fn name_cudevice_unicode(device: CuDevice, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCuDeviceW(device, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA context with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA context.
pub unsafe fn name_cucontext_ascii(context: CuContext, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCuContextA(context, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA context with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA context.
pub unsafe fn name_cucontext_unicode(context: CuContext, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCuContextW(context, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA stream with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_custream_ascii(stream: CuStream, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCuStreamA(stream, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA stream with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_custream_unicode(stream: CuStream, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCuStreamW(stream, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA event with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuevent_ascii(event: CuEvent, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCuEventA(event, name.as_ptr()) }
}

#[cfg(feature = "cuda")]
/// Name a CUDA event with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuevent_unicode(event: CuEvent, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCuEventW(event, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime device with an ASCII string.
pub fn name_cuda_device_ascii(device: i32, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCudaDeviceA(device, name.as_ptr()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime device with a Unicode string.
pub fn name_cuda_device_unicode(device: i32, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCudaDeviceW(device, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime stream with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_cuda_stream_ascii(stream: CudaStream, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCudaStreamA(stream, name.as_ptr()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime stream with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA stream.
pub unsafe fn name_cuda_stream_unicode(stream: CudaStream, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCudaStreamW(stream, name.as_ptr().cast()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime event with an ASCII string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuda_event_ascii(event: CudaEvent, name: &CStr) {
    unsafe { crate::ffi::nvtxNameCudaEventA(event, name.as_ptr()) }
}

#[cfg(feature = "cuda_runtime")]
/// Name a CUDA Runtime event with a Unicode string.
///
/// # Safety
/// This function is marked unsafe because of the pointer parameter referring to the CUDA event.
pub unsafe fn name_cuda_event_unicode(event: CudaEvent, name: &WideCStr) {
    unsafe { crate::ffi::nvtxNameCudaEventW(event, name.as_ptr().cast()) }
}

#[must_use]
/// Create a new user-defined synchronization within a domain.
pub fn domain_syncuser_create(domain: DomainHandle, attribs: SyncUserAttributes) -> SyncUserHandle {
    SyncUserHandle {
        handle: unsafe {
            crate::ffi::nvtxDomainSyncUserCreate(
                domain.handle,
                std::ptr::addr_of!(attribs).cast_mut(),
            )
        },
    }
}

/// Destroy a user-defined synchronization.
///
/// Created by [`domain_syncuser_create`].
pub fn domain_syncuser_destroy(handle: SyncUserHandle) {
    unsafe { crate::ffi::nvtxDomainSyncUserDestroy(handle.handle) }
}

/// Indicate that a user-defined synchronization started to acquire.
pub fn domain_syncuser_acquire_start(handle: SyncUserHandle) {
    unsafe { crate::ffi::nvtxDomainSyncUserAcquireStart(handle.handle) }
}

/// Indicate that a user-defined synchronization acquisition failed.
///
/// Note: this call is only valid after a call to [`domain_syncuser_acquire_start`].
pub fn domain_syncuser_acquire_failed(handle: SyncUserHandle) {
    unsafe { crate::ffi::nvtxDomainSyncUserAcquireFailed(handle.handle) }
}

/// Indicate that a user-defined synchronization acquisition succeeded.
///
/// Note: this call is only valid after a call to [`domain_syncuser_acquire_start`].
pub fn domain_syncuser_acquire_success(handle: SyncUserHandle) {
    unsafe { crate::ffi::nvtxDomainSyncUserAcquireSuccess(handle.handle) }
}

/// Indicate that a user-defined synchronization is released.
///
/// Note: this call is only valid after a call to [`domain_syncuser_acquire_success`].
pub fn domain_syncuser_acquire_releasing(handle: SyncUserHandle) {
    unsafe { crate::ffi::nvtxDomainSyncUserReleasing(handle.handle) }
}

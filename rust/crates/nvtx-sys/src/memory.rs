// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::{ffi, DomainHandle};

pub type MemoryRegionRef = ffi::nvtxMemRegionRef_t;
pub type VirtualRangeDescriptor = ffi::nvtxMemVirtualRangeDesc_t;
pub type HeapDescriptor = ffi::nvtxMemHeapDesc_t;
pub type RegionsRegisterBatch = ffi::nvtxMemRegionsRegisterBatch_t;
pub type RegionsResizeBatch = ffi::nvtxMemRegionsResizeBatch_t;
pub type RegionsUnregisterBatch = ffi::nvtxMemRegionsUnregisterBatch_t;
pub type RegionNameDescriptor = ffi::nvtxMemRegionNameDesc_t;
pub type RegionsNameBatch = ffi::nvtxMemRegionsNameBatch_t;
pub type PermissionsAssignRegionDescriptor = ffi::nvtxMemPermissionsAssignRegionDesc_t;
pub type PermissionsAssignBatch = ffi::nvtxMemPermissionsAssignBatch_t;

#[cfg(feature = "memory_cuda_runtime")]
pub type CudaArrayRangeDescriptor = ffi::nvtxMemCudaArrayRangeDesc_t;
#[cfg(feature = "memory_cuda_runtime")]
pub type CuArrayRangeDescriptor = ffi::nvtxMemCuArrayRangeDesc_t;
#[cfg(feature = "memory_cuda_runtime")]
pub type MarkInitializedBatch = ffi::nvtxMemMarkInitializedBatch_t;
#[cfg(feature = "memory_cuda_runtime")]
pub type CudaArray = ffi::cudaArray_t;
#[cfg(feature = "memory_cuda_runtime")]
pub type CuArray = ffi::CUarray;

/// Opaque memory heap handle.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MemoryHeapHandle {
    raw: ffi::nvtxMemHeapHandle_t,
}

impl MemoryHeapHandle {
    /// Return the special process-wide virtual-address heap.
    #[must_use]
    pub const fn process_wide() -> Self {
        Self {
            raw: core::ptr::null_mut(),
        }
    }

    /// Construct a handle from its raw representation.
    ///
    /// # Safety
    /// `raw` must be an NVTX memory heap handle or documented sentinel.
    #[must_use]
    pub const unsafe fn from_raw(raw: ffi::nvtxMemHeapHandle_t) -> Self {
        Self { raw }
    }

    /// Return the raw FFI handle.
    #[must_use]
    pub const fn as_raw(self) -> ffi::nvtxMemHeapHandle_t {
        self.raw
    }

    /// Return whether this is the null process-wide handle.
    #[must_use]
    pub fn is_null(self) -> bool {
        self.raw.is_null()
    }

    /// Return whether a tool was unavailable when this handle was produced.
    #[must_use]
    pub fn is_no_tool(self) -> bool {
        self.raw.cast::<u8>() as isize == -1
    }
}

/// Opaque memory region handle.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MemoryRegionHandle {
    raw: ffi::nvtxMemRegionHandle_t,
}

impl MemoryRegionHandle {
    /// Construct a handle from its raw representation.
    ///
    /// # Safety
    /// `raw` must be an NVTX memory region handle or documented sentinel.
    #[must_use]
    pub const unsafe fn from_raw(raw: ffi::nvtxMemRegionHandle_t) -> Self {
        Self { raw }
    }

    /// Return the raw FFI handle.
    #[must_use]
    pub const fn as_raw(self) -> ffi::nvtxMemRegionHandle_t {
        self.raw
    }

    /// Return whether this handle is null.
    #[must_use]
    pub fn is_null(self) -> bool {
        self.raw.is_null()
    }

    /// Return whether a tool was unavailable when this handle was produced.
    #[must_use]
    pub fn is_no_tool(self) -> bool {
        self.raw.cast::<u8>() as isize == -1
    }
}

/// Opaque memory permissions handle.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MemoryPermissionsHandle {
    raw: ffi::nvtxMemPermissionsHandle_t,
}

impl MemoryPermissionsHandle {
    /// Return the special process-wide permissions object.
    #[must_use]
    pub const fn process_wide() -> Self {
        Self {
            raw: core::ptr::null_mut(),
        }
    }

    /// Construct a handle from its raw representation.
    ///
    /// # Safety
    /// `raw` must be an NVTX memory permissions handle or documented sentinel.
    #[must_use]
    pub const unsafe fn from_raw(raw: ffi::nvtxMemPermissionsHandle_t) -> Self {
        Self { raw }
    }

    /// Return the raw FFI handle.
    #[must_use]
    pub const fn as_raw(self) -> ffi::nvtxMemPermissionsHandle_t {
        self.raw
    }

    /// Return whether this is the null process-wide handle.
    #[must_use]
    pub fn is_null(self) -> bool {
        self.raw.is_null()
    }

    /// Return whether a tool was unavailable when this handle was produced.
    #[must_use]
    pub fn is_no_tool(self) -> bool {
        self.raw.cast::<u8>() as isize == -1
    }
}

/// Register a memory heap.
///
/// # Safety
/// Every pointer reachable from `descriptor` must remain valid for the call and
/// match the selected memory type.
#[must_use]
pub unsafe fn heap_register(domain: DomainHandle, descriptor: &HeapDescriptor) -> MemoryHeapHandle {
    // SAFETY: The caller guarantees the descriptor graph is valid.
    let raw = unsafe { ffi::nvtxMemHeapRegister(domain.handle, descriptor) };
    MemoryHeapHandle { raw }
}

/// Unregister a memory heap.
pub fn heap_unregister(domain: DomainHandle, heap: MemoryHeapHandle) {
    // SAFETY: The typed handle originates from NVTX or a documented sentinel.
    unsafe { ffi::nvtxMemHeapUnregister(domain.handle, heap.raw) }
}

/// Reset a memory heap.
pub fn heap_reset(domain: DomainHandle, heap: MemoryHeapHandle) {
    // SAFETY: The typed handle originates from NVTX or a documented sentinel.
    unsafe { ffi::nvtxMemHeapReset(domain.handle, heap.raw) }
}

/// Register memory regions.
///
/// # Safety
/// Every input and output pointer reachable from `batch` must be valid for the
/// declared element count and element size.
pub unsafe fn regions_register(domain: DomainHandle, batch: &RegionsRegisterBatch) {
    // SAFETY: The caller guarantees descriptor and output pointer validity.
    unsafe { ffi::nvtxMemRegionsRegister(domain.handle, batch) }
}

/// Resize memory regions.
///
/// # Safety
/// Every pointer reachable from `batch` must be valid for the declared element
/// count and size.
pub unsafe fn regions_resize(domain: DomainHandle, batch: &RegionsResizeBatch) {
    // SAFETY: The caller guarantees descriptor pointer validity.
    unsafe { ffi::nvtxMemRegionsResize(domain.handle, batch) }
}

/// Unregister memory regions.
///
/// # Safety
/// Every region reference in `batch` must be valid for the selected reference
/// type.
pub unsafe fn regions_unregister(domain: DomainHandle, batch: &RegionsUnregisterBatch) {
    // SAFETY: The caller guarantees region reference validity.
    unsafe { ffi::nvtxMemRegionsUnregister(domain.handle, batch) }
}

/// Name memory regions.
///
/// # Safety
/// Every region and string pointer reachable from `batch` must be valid for
/// the duration of the call.
pub unsafe fn regions_name(domain: DomainHandle, batch: &RegionsNameBatch) {
    // SAFETY: The caller guarantees descriptor pointer validity.
    unsafe { ffi::nvtxMemRegionsName(domain.handle, batch) }
}

/// Assign permissions to memory regions.
///
/// # Safety
/// Every region reference reachable from `batch` must be valid and compatible
/// with its declared reference type.
pub unsafe fn permissions_assign(domain: DomainHandle, batch: &PermissionsAssignBatch) {
    // SAFETY: The caller guarantees descriptor pointer validity.
    unsafe { ffi::nvtxMemPermissionsAssign(domain.handle, batch) }
}

/// Create a memory permissions object.
#[must_use]
pub fn permissions_create(domain: DomainHandle, creation_flags: i32) -> MemoryPermissionsHandle {
    // SAFETY: All arguments are scalar or opaque handles.
    let raw = unsafe { ffi::nvtxMemPermissionsCreate(domain.handle, creation_flags) };
    MemoryPermissionsHandle { raw }
}

/// Destroy a created memory permissions object.
pub fn permissions_destroy(domain: DomainHandle, permissions: MemoryPermissionsHandle) {
    // SAFETY: The typed handle originates from NVTX.
    unsafe { ffi::nvtxMemPermissionsDestroy(domain.handle, permissions.raw) }
}

/// Reset a memory permissions object.
pub fn permissions_reset(domain: DomainHandle, permissions: MemoryPermissionsHandle) {
    // SAFETY: The typed handle originates from NVTX or is a documented sentinel.
    unsafe { ffi::nvtxMemPermissionsReset(domain.handle, permissions.raw) }
}

/// Bind a memory permissions object to a scope on the current thread.
pub fn permissions_bind(
    domain: DomainHandle,
    permissions: MemoryPermissionsHandle,
    scope: u32,
    flags: u32,
) {
    // SAFETY: The typed handle originates from NVTX and remaining arguments are scalars.
    unsafe { ffi::nvtxMemPermissionsBind(domain.handle, permissions.raw, scope, flags) }
}

/// Unbind memory permissions from a scope on the current thread.
pub fn permissions_unbind(domain: DomainHandle, scope: u32) {
    // SAFETY: Arguments are an opaque domain handle and scalar scope.
    unsafe { ffi::nvtxMemPermissionsUnbind(domain.handle, scope) }
}

/// Get the CUDA process-wide permissions object.
#[cfg(feature = "memory_cuda_runtime")]
#[must_use]
pub fn cuda_process_wide_permissions(domain: DomainHandle) -> MemoryPermissionsHandle {
    // SAFETY: `domain` is an opaque handle created by NVTX.
    let raw = unsafe { ffi::nvtxMemCudaGetProcessWidePermissions(domain.handle) };
    MemoryPermissionsHandle { raw }
}

/// Get a CUDA device-wide permissions object.
#[cfg(feature = "memory_cuda_runtime")]
#[must_use]
pub fn cuda_device_wide_permissions(domain: DomainHandle, device: i32) -> MemoryPermissionsHandle {
    // SAFETY: Arguments are an opaque domain handle and scalar device ID.
    let raw = unsafe { ffi::nvtxMemCudaGetDeviceWidePermissions(domain.handle, device) };
    MemoryPermissionsHandle { raw }
}

/// Set CUDA peer-device access for a permissions object.
#[cfg(feature = "memory_cuda_runtime")]
pub fn cuda_set_peer_access(
    domain: DomainHandle,
    permissions: MemoryPermissionsHandle,
    peer_device: i32,
    flags: u32,
) {
    // SAFETY: The typed handle originates from NVTX and other arguments are scalars.
    unsafe {
        ffi::nvtxMemCudaSetPeerAccess(domain.handle, permissions.raw, peer_device, flags);
    }
}

/// Mark CUDA memory regions initialized on a stream.
///
/// # Safety
/// `stream` must be a valid CUDA Runtime stream and every descriptor pointer
/// reachable from `batch` must be valid for the call.
#[cfg(feature = "memory_cuda_runtime")]
pub unsafe fn cuda_mark_initialized(
    domain: DomainHandle,
    stream: crate::CudaStream,
    is_per_thread_stream: bool,
    batch: &MarkInitializedBatch,
) {
    // SAFETY: The caller guarantees stream and descriptor pointer validity.
    unsafe {
        ffi::nvtxMemCudaMarkInitialized(
            domain.handle,
            stream,
            u8::from(is_per_thread_stream),
            batch,
        );
    }
}

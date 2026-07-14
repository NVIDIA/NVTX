// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use alloc::{vec, vec::Vec};
use core::{
    ffi::{c_void, CStr},
    marker::PhantomData,
    mem::{size_of, size_of_val},
    ptr::{addr_of, null_mut},
};

use crate::Domain;

/// Error returned by an ergonomic memory annotation API.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum MemoryError {
    /// A reset permission was combined with an access permission.
    ResetWithAccess,
    /// An index did not identify a registered region.
    RegionOutOfBounds,
    /// Parallel slices had different lengths.
    LengthMismatch,
    /// A region list was empty.
    EmptyRegions,
}

impl core::fmt::Display for MemoryError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::ResetWithAccess => write!(f, "reset cannot be combined with access permissions"),
            Self::RegionOutOfBounds => write!(f, "memory region index is out of bounds"),
            Self::LengthMismatch => write!(f, "memory descriptor slice lengths differ"),
            Self::EmptyRegions => write!(f, "at least one memory region is required"),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for MemoryError {}

/// Intended use of a registered memory heap.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MemoryHeapUsage {
    /// Heap managed by a sub-allocator.
    SubAllocator,
    /// Heap with an explicit layout.
    Layout,
}

impl MemoryHeapUsage {
    const fn raw(self) -> u32 {
        match self {
            Self::SubAllocator => nvtx_sys::ffi::NVTX_MEM_HEAP_USAGE_TYPE_SUB_ALLOCATOR as u32,
            Self::Layout => nvtx_sys::ffi::NVTX_MEM_HEAP_USAGE_TYPE_LAYOUT as u32,
        }
    }
}

/// Borrowed virtual address range.
#[derive(Debug, Clone, Copy)]
pub struct VirtualRange<'a> {
    pointer: *const c_void,
    size: usize,
    lifetime: PhantomData<&'a [u8]>,
}

impl<'a> VirtualRange<'a> {
    /// Describe a byte slice as virtual memory.
    #[must_use]
    pub fn from_slice(bytes: &'a [u8]) -> Self {
        Self {
            pointer: bytes.as_ptr().cast(),
            size: bytes.len(),
            lifetime: PhantomData,
        }
    }

    /// Describe an arbitrary virtual address range.
    ///
    /// # Safety
    /// `pointer..pointer + size` must identify the intended live allocation for
    /// every NVTX annotation that retains this descriptor.
    #[must_use]
    pub const unsafe fn from_raw_parts(pointer: *const c_void, size: usize) -> Self {
        Self {
            pointer,
            size,
            lifetime: PhantomData,
        }
    }

    fn raw(self) -> nvtx_sys::memory::VirtualRangeDescriptor {
        nvtx_sys::memory::VirtualRangeDescriptor {
            size: self.size,
            ptr: self.pointer,
        }
    }
}

enum HeapStorage<'a> {
    Virtual(VirtualRange<'a>),
    #[cfg(feature = "memory_cuda_runtime")]
    CudaArray(nvtx_sys::memory::CudaArray),
    #[cfg(feature = "memory_cuda_runtime")]
    CuArray(nvtx_sys::memory::CuArray),
}

/// Description used to register a memory heap.
pub struct MemoryHeapDescriptor<'a> {
    usage: MemoryHeapUsage,
    storage: HeapStorage<'a>,
    name: Option<&'a CStr>,
    category: u32,
}

impl<'a> MemoryHeapDescriptor<'a> {
    /// Describe a virtual-address heap.
    #[must_use]
    pub const fn virtual_address(usage: MemoryHeapUsage, range: VirtualRange<'a>) -> Self {
        Self {
            usage,
            storage: HeapStorage::Virtual(range),
            name: None,
            category: 0,
        }
    }

    /// Describe a CUDA Runtime array heap.
    ///
    /// # Safety
    /// `array` must be a valid CUDA Runtime array handle for every annotation
    /// that retains the registered heap.
    #[cfg(feature = "memory_cuda_runtime")]
    #[must_use]
    pub const unsafe fn cuda_array(
        usage: MemoryHeapUsage,
        array: nvtx_sys::memory::CudaArray,
    ) -> Self {
        Self {
            usage,
            storage: HeapStorage::CudaArray(array),
            name: None,
            category: 0,
        }
    }

    /// Describe a CUDA Driver array heap.
    ///
    /// # Safety
    /// `array` must be a valid CUDA Driver array handle for every annotation
    /// that retains the registered heap.
    #[cfg(feature = "memory_cuda_runtime")]
    #[must_use]
    pub const unsafe fn cu_array(usage: MemoryHeapUsage, array: nvtx_sys::memory::CuArray) -> Self {
        Self {
            usage,
            storage: HeapStorage::CuArray(array),
            name: None,
            category: 0,
        }
    }

    /// Set an ASCII heap name.
    #[must_use]
    pub const fn name(mut self, name: &'a CStr) -> Self {
        self.name = Some(name);
        self
    }

    /// Set a category ID.
    #[must_use]
    pub const fn category(mut self, category: u32) -> Self {
        self.category = category;
        self
    }
}

/// Registered memory heap.
pub struct MemoryHeap<'a> {
    domain: &'a Domain,
    handle: nvtx_sys::memory::MemoryHeapHandle,
    owned: bool,
}

impl<'domain> MemoryHeap<'domain> {
    /// Return whether no NVTX tool handled registration.
    #[must_use]
    pub fn is_no_tool(&self) -> bool {
        self.handle.is_no_tool()
    }

    /// Reset all annotations associated with the heap.
    pub fn reset(&self) {
        nvtx_sys::memory::heap_reset(self.domain.raw_handle(), self.handle);
    }

    /// Register virtual-address regions within this heap.
    ///
    /// # Errors
    /// Returns [`MemoryError::EmptyRegions`] when `regions` is empty.
    pub fn register_virtual_regions<'heap, 'memory>(
        &'heap self,
        regions: &[VirtualRange<'memory>],
    ) -> Result<RegisteredRegions<'heap, 'domain, 'memory>, MemoryError> {
        let raw: Vec<_> = regions.iter().copied().map(VirtualRange::raw).collect();
        self.register_regions(nvtx_sys::ffi::NVTX_MEM_TYPE_VIRTUAL_ADDRESS as u32, &raw)
    }

    /// Register CUDA Runtime array regions within this heap.
    ///
    /// # Errors
    /// Returns [`MemoryError::EmptyRegions`] when `regions` is empty.
    #[cfg(feature = "memory_cuda_runtime")]
    pub fn register_cuda_array_regions<'heap>(
        &'heap self,
        regions: &[CudaArrayRange],
    ) -> Result<RegisteredRegions<'heap, 'domain, 'static>, MemoryError> {
        let raw: Vec<_> = regions.iter().map(CudaArrayRange::raw).collect();
        self.register_regions(nvtx_sys::ffi::NVTX_MEM_TYPE_CUDA_ARRAY as u32, &raw)
    }

    /// Register CUDA Driver array regions within this heap.
    ///
    /// # Errors
    /// Returns [`MemoryError::EmptyRegions`] when `regions` is empty.
    #[cfg(feature = "memory_cuda_runtime")]
    pub fn register_cu_array_regions<'heap>(
        &'heap self,
        regions: &[CuArrayRange],
    ) -> Result<RegisteredRegions<'heap, 'domain, 'static>, MemoryError> {
        let raw: Vec<_> = regions.iter().map(CuArrayRange::raw).collect();
        self.register_regions(nvtx_sys::ffi::NVTX_MEM_TYPE_CU_ARRAY as u32, &raw)
    }

    fn register_regions<'heap, 'memory, T>(
        &'heap self,
        region_type: u32,
        regions: &[T],
    ) -> Result<RegisteredRegions<'heap, 'domain, 'memory>, MemoryError> {
        if regions.is_empty() {
            return Err(MemoryError::EmptyRegions);
        }
        let mut outputs = vec![null_mut(); regions.len()];
        let batch = nvtx_sys::memory::RegionsRegisterBatch {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::RegionsRegisterBatch>() as u16,
            regionType: region_type,
            heap: self.handle.as_raw(),
            regionCount: regions.len(),
            regionDescElementSize: size_of::<T>(),
            regionDescElements: regions.as_ptr().cast(),
            regionHandleElementsOut: outputs.as_mut_ptr(),
        };
        // SAFETY: `regions` and `outputs` are valid contiguous buffers matching
        // the declared counts and element sizes.
        unsafe { nvtx_sys::memory::regions_register(self.domain.raw_handle(), &batch) }
        let handles = outputs
            .into_iter()
            .map(|raw| {
                // SAFETY: Every output slot is initialized to null and may be
                // replaced only by NVTX with a documented region handle.
                unsafe { nvtx_sys::memory::MemoryRegionHandle::from_raw(raw) }
            })
            .collect();
        Ok(RegisteredRegions {
            heap: self,
            handles,
            memory: PhantomData,
        })
    }
}

impl Drop for MemoryHeap<'_> {
    fn drop(&mut self) {
        if self.owned && !self.handle.is_null() && !self.handle.is_no_tool() {
            nvtx_sys::memory::heap_unregister(self.domain.raw_handle(), self.handle);
        }
    }
}

/// CUDA Runtime array subrange.
#[cfg(feature = "memory_cuda_runtime")]
#[derive(Debug, Clone, Copy)]
pub struct CudaArrayRange {
    array: nvtx_sys::memory::CudaArray,
    offset: [usize; 3],
    extent: [usize; 3],
}

#[cfg(feature = "memory_cuda_runtime")]
impl CudaArrayRange {
    /// Describe a CUDA Runtime array range.
    ///
    /// # Safety
    /// `array` must be a valid CUDA Runtime array handle covering the requested
    /// offset and extent.
    #[must_use]
    pub const unsafe fn new(
        array: nvtx_sys::memory::CudaArray,
        offset: [usize; 3],
        extent: [usize; 3],
    ) -> Self {
        Self {
            array,
            offset,
            extent,
        }
    }

    fn raw(&self) -> nvtx_sys::memory::CudaArrayRangeDescriptor {
        nvtx_sys::memory::CudaArrayRangeDescriptor {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::CudaArrayRangeDescriptor>() as u16,
            reserved0: 0,
            src: self.array,
            offset: self.offset,
            extent: self.extent,
        }
    }
}

/// CUDA Driver array subrange.
#[cfg(feature = "memory_cuda_runtime")]
#[derive(Debug, Clone, Copy)]
pub struct CuArrayRange {
    array: nvtx_sys::memory::CuArray,
    offset: [usize; 3],
    extent: [usize; 3],
}

#[cfg(feature = "memory_cuda_runtime")]
impl CuArrayRange {
    /// Describe a CUDA Driver array range.
    ///
    /// # Safety
    /// `array` must be a valid CUDA Driver array handle covering the requested
    /// offset and extent.
    #[must_use]
    pub const unsafe fn new(
        array: nvtx_sys::memory::CuArray,
        offset: [usize; 3],
        extent: [usize; 3],
    ) -> Self {
        Self {
            array,
            offset,
            extent,
        }
    }

    fn raw(&self) -> nvtx_sys::memory::CuArrayRangeDescriptor {
        nvtx_sys::memory::CuArrayRangeDescriptor {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::CuArrayRangeDescriptor>() as u16,
            reserved0: 0,
            src: self.array,
            offset: self.offset,
            extent: self.extent,
        }
    }
}

/// Reference to a registered memory region.
#[derive(Clone, Copy)]
pub struct MemoryRegionRef<'a> {
    raw: nvtx_sys::memory::MemoryRegionRef,
    reference_type: u32,
    lifetime: PhantomData<&'a ()>,
}

impl MemoryRegionRef<'_> {
    /// Reference an arbitrary address.
    ///
    /// # Safety
    /// `pointer` must identify the intended live region whenever this reference
    /// is submitted to NVTX.
    #[must_use]
    pub const unsafe fn from_pointer(pointer: *const c_void) -> Self {
        Self {
            raw: nvtx_sys::memory::MemoryRegionRef { pointer },
            reference_type: nvtx_sys::ffi::NVTX_MEM_REGION_REF_TYPE_POINTER as u32,
            lifetime: PhantomData,
        }
    }

    fn from_handle(handle: nvtx_sys::memory::MemoryRegionHandle) -> Self {
        Self {
            raw: nvtx_sys::memory::MemoryRegionRef {
                handle: handle.as_raw(),
            },
            reference_type: nvtx_sys::ffi::NVTX_MEM_REGION_REF_TYPE_HANDLE as u32,
            lifetime: PhantomData,
        }
    }
}

impl core::fmt::Debug for MemoryRegionRef<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("MemoryRegionRef")
            .field("reference_type", &self.reference_type)
            .finish_non_exhaustive()
    }
}

/// Group of regions registered against a heap.
pub struct RegisteredRegions<'heap, 'domain, 'memory>
where
    'domain: 'heap,
{
    heap: &'heap MemoryHeap<'domain>,
    handles: Vec<nvtx_sys::memory::MemoryRegionHandle>,
    memory: PhantomData<&'memory [u8]>,
}

impl RegisteredRegions<'_, '_, '_> {
    /// Number of registered region handles.
    #[must_use]
    pub fn len(&self) -> usize {
        self.handles.len()
    }

    /// Return whether no regions are represented.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.handles.is_empty()
    }

    /// Return one region reference.
    #[must_use]
    pub fn region(&self, index: usize) -> Option<MemoryRegionRef<'_>> {
        self.handles
            .get(index)
            .copied()
            .map(MemoryRegionRef::from_handle)
    }

    /// Resize virtual-address regions in place.
    ///
    /// # Errors
    /// Returns [`MemoryError::LengthMismatch`] when `regions` does not contain
    /// one descriptor per registered region.
    ///
    /// # Safety
    /// Every resized range's backing allocation must remain valid until these
    /// registered regions are dropped or resized again.
    pub unsafe fn resize_virtual_regions(
        &self,
        regions: &[VirtualRange<'_>],
    ) -> Result<(), MemoryError> {
        if regions.len() != self.handles.len() {
            return Err(MemoryError::LengthMismatch);
        }
        let descriptors: Vec<_> = regions.iter().copied().map(VirtualRange::raw).collect();
        let batch = nvtx_sys::memory::RegionsResizeBatch {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::RegionsResizeBatch>() as u16,
            regionType: nvtx_sys::ffi::NVTX_MEM_TYPE_VIRTUAL_ADDRESS as u32,
            regionDescCount: descriptors.len(),
            regionDescElementSize: size_of::<nvtx_sys::memory::VirtualRangeDescriptor>(),
            regionDescElements: descriptors.as_ptr().cast(),
        };
        // SAFETY: The descriptor slice remains alive for the complete call.
        unsafe { nvtx_sys::memory::regions_resize(self.heap.domain.raw_handle(), &batch) }
        Ok(())
    }

    /// Set ASCII names for every region.
    ///
    /// # Errors
    /// Returns [`MemoryError::LengthMismatch`] when `names` does not contain
    /// one name per registered region.
    pub fn name_all(&self, names: &[&CStr], category: u32) -> Result<(), MemoryError> {
        if names.len() != self.handles.len() {
            return Err(MemoryError::LengthMismatch);
        }
        let descriptors: Vec<_> = self
            .handles
            .iter()
            .zip(names)
            .map(|(handle, name)| nvtx_sys::memory::RegionNameDescriptor {
                regionRefType: nvtx_sys::ffi::NVTX_MEM_REGION_REF_TYPE_HANDLE as u32,
                nameType: nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_TYPE_ASCII as u32,
                region: nvtx_sys::memory::MemoryRegionRef {
                    handle: handle.as_raw(),
                },
                name: nvtx_sys::ffi::nvtxMessageValue_t {
                    ascii: name.as_ptr(),
                },
                category,
                reserved0: 0,
            })
            .collect();
        let batch = nvtx_sys::memory::RegionsNameBatch {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::RegionsNameBatch>() as u16,
            reserved0: 0,
            regionCount: descriptors.len(),
            regionElementSize: size_of::<nvtx_sys::memory::RegionNameDescriptor>(),
            regionElements: descriptors.as_ptr(),
            reserved1: 0,
        };
        // SAFETY: Every handle and C string is valid for the call duration.
        unsafe { nvtx_sys::memory::regions_name(self.heap.domain.raw_handle(), &batch) }
        Ok(())
    }
}

impl Drop for RegisteredRegions<'_, '_, '_> {
    fn drop(&mut self) {
        let references: Vec<_> = self
            .handles
            .iter()
            .copied()
            .filter(|handle| !handle.is_null() && !handle.is_no_tool())
            .map(|handle| nvtx_sys::memory::MemoryRegionRef {
                handle: handle.as_raw(),
            })
            .collect();
        if references.is_empty() {
            return;
        }
        let batch = nvtx_sys::memory::RegionsUnregisterBatch {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::RegionsUnregisterBatch>() as u16,
            refType: nvtx_sys::ffi::NVTX_MEM_REGION_REF_TYPE_HANDLE as u32,
            refCount: references.len(),
            refElementSize: size_of::<nvtx_sys::memory::MemoryRegionRef>(),
            refElements: references.as_ptr(),
        };
        // SAFETY: Every reference comes from NVTX and remains valid until this
        // unregister call.
        unsafe { nvtx_sys::memory::regions_unregister(self.heap.domain.raw_handle(), &batch) }
    }
}

/// Access permissions assigned to a memory region.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct PermissionAccess {
    bits: u32,
    reset: bool,
}

impl PermissionAccess {
    /// Start with no permissions.
    #[must_use]
    pub const fn none() -> Self {
        Self {
            bits: 0,
            reset: false,
        }
    }

    /// Allow reads.
    #[must_use]
    pub const fn read(mut self) -> Self {
        self.bits |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_REGION_FLAGS_READ as u32;
        self
    }

    /// Allow writes.
    #[must_use]
    pub const fn write(mut self) -> Self {
        self.bits |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_REGION_FLAGS_WRITE as u32;
        self
    }

    /// Allow atomic read-modify-write operations.
    #[must_use]
    pub const fn atomic(mut self) -> Self {
        self.bits |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_REGION_FLAGS_ATOMIC as u32;
        self
    }

    /// Reset annotations for the region.
    #[must_use]
    pub const fn reset(mut self) -> Self {
        self.reset = true;
        self
    }

    fn raw(self) -> Result<u32, MemoryError> {
        if self.reset && self.bits != 0 {
            return Err(MemoryError::ResetWithAccess);
        }
        Ok(if self.reset {
            nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_REGION_FLAGS_RESET as u32
        } else {
            self.bits
        })
    }
}

/// One permissions assignment.
#[derive(Debug, Clone, Copy)]
pub struct PermissionAssignment<'a> {
    region: MemoryRegionRef<'a>,
    access: PermissionAccess,
}

impl<'a> PermissionAssignment<'a> {
    /// Assign access to a region.
    #[must_use]
    pub const fn new(region: MemoryRegionRef<'a>, access: PermissionAccess) -> Self {
        Self { region, access }
    }
}

/// Flags used while creating a permissions object.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct PermissionCreationFlags(u32);

impl PermissionCreationFlags {
    /// Start with normal global inheritance.
    #[must_use]
    pub const fn new() -> Self {
        Self(0)
    }

    /// Exclude globally readable regions.
    #[must_use]
    pub const fn exclude_global_read(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_CREATE_FLAGS_EXCLUDE_GLOBAL_READ as u32;
        self
    }

    /// Exclude globally writable regions.
    #[must_use]
    pub const fn exclude_global_write(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_CREATE_FLAGS_EXCLUDE_GLOBAL_WRITE as u32;
        self
    }

    /// Exclude globally atomic regions.
    #[must_use]
    pub const fn exclude_global_atomic(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_CREATE_FLAGS_EXCLUDE_GLOBAL_ATOMIC as u32;
        self
    }
}

/// Scope to which memory permissions are bound.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PermissionBindScope {
    /// CPU work on the current thread.
    CpuThread,
    /// CUDA work enqueued from the current thread.
    CudaStream,
}

impl PermissionBindScope {
    const fn raw(self) -> u32 {
        match self {
            Self::CpuThread => nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_BIND_SCOPE_CPU_THREAD as u32,
            Self::CudaStream => nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_BIND_SCOPE_CUDA_STREAM as u32,
        }
    }
}

/// Inheritance behavior while binding permissions.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct PermissionBindFlags(u32);

impl PermissionBindFlags {
    /// Start with overlay inheritance.
    #[must_use]
    pub const fn new() -> Self {
        Self(0)
    }

    /// Exclude inherited reads.
    #[must_use]
    pub const fn strict_read(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_BIND_FLAGS_STRICT_READ as u32;
        self
    }

    /// Exclude inherited writes.
    #[must_use]
    pub const fn strict_write(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_BIND_FLAGS_STRICT_WRITE as u32;
        self
    }

    /// Exclude inherited atomic access.
    #[must_use]
    pub const fn strict_atomic(mut self) -> Self {
        self.0 |= nvtx_sys::ffi::NVTX_MEM_PERMISSIONS_BIND_FLAGS_STRICT_ATOMIC as u32;
        self
    }
}

/// Domain-bound memory permissions object.
pub struct MemoryPermissions<'a> {
    domain: &'a Domain,
    handle: nvtx_sys::memory::MemoryPermissionsHandle,
    owned: bool,
}

impl MemoryPermissions<'_> {
    /// Return whether no NVTX tool handled creation.
    #[must_use]
    pub fn is_no_tool(&self) -> bool {
        self.handle.is_no_tool()
    }

    /// Reset the permissions object.
    pub fn reset(&self) {
        nvtx_sys::memory::permissions_reset(self.domain.raw_handle(), self.handle);
    }

    /// Assign access to memory regions.
    ///
    /// # Errors
    /// Returns [`MemoryError::ResetWithAccess`] when an assignment combines
    /// reset with read, write, or atomic access.
    pub fn assign(&self, assignments: &[PermissionAssignment<'_>]) -> Result<(), MemoryError> {
        let raw: Result<Vec<_>, _> = assignments
            .iter()
            .map(|assignment| {
                Ok(nvtx_sys::memory::PermissionsAssignRegionDescriptor {
                    flags: assignment.access.raw()?,
                    regionRefType: assignment.region.reference_type,
                    region: assignment.region.raw,
                })
            })
            .collect();
        let raw = raw?;
        let batch = nvtx_sys::memory::PermissionsAssignBatch {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::PermissionsAssignBatch>() as u16,
            reserved0: 0,
            permissions: self.handle.as_raw(),
            regionCount: raw.len(),
            regionElementSize: size_of::<nvtx_sys::memory::PermissionsAssignRegionDescriptor>(),
            regionElements: raw.as_ptr(),
            reserved1: 0,
        };
        // SAFETY: Every region reference and descriptor is valid for the call.
        unsafe { nvtx_sys::memory::permissions_assign(self.domain.raw_handle(), &batch) }
        Ok(())
    }

    /// Bind permissions to the current thread and return an unbinding guard.
    #[must_use]
    pub fn bind(
        &self,
        scope: PermissionBindScope,
        flags: PermissionBindFlags,
    ) -> PermissionBinding<'_> {
        nvtx_sys::memory::permissions_bind(
            self.domain.raw_handle(),
            self.handle,
            scope.raw(),
            flags.0,
        );
        PermissionBinding {
            permissions: self,
            scope,
            not_send: PhantomData,
        }
    }

    /// Set peer-device access on a CUDA permissions object.
    ///
    /// # Errors
    /// Returns [`MemoryError::ResetWithAccess`] when `access` combines reset
    /// with read, write, or atomic access.
    #[cfg(feature = "memory_cuda_runtime")]
    pub fn set_cuda_peer_access(
        &self,
        peer_device: Option<i32>,
        access: PermissionAccess,
    ) -> Result<(), MemoryError> {
        nvtx_sys::memory::cuda_set_peer_access(
            self.domain.raw_handle(),
            self.handle,
            peer_device.unwrap_or(nvtx_sys::ffi::NVTX_MEM_CUDA_PEER_ALL_DEVICES),
            access.raw()?,
        );
        Ok(())
    }
}

impl Drop for MemoryPermissions<'_> {
    fn drop(&mut self) {
        if self.owned && !self.handle.is_null() && !self.handle.is_no_tool() {
            nvtx_sys::memory::permissions_destroy(self.domain.raw_handle(), self.handle);
        }
    }
}

/// Thread-local guard that unbinds permissions when dropped.
pub struct PermissionBinding<'a> {
    permissions: &'a MemoryPermissions<'a>,
    scope: PermissionBindScope,
    not_send: PhantomData<*mut ()>,
}

impl Drop for PermissionBinding<'_> {
    fn drop(&mut self) {
        nvtx_sys::memory::permissions_unbind(
            self.permissions.domain.raw_handle(),
            self.scope.raw(),
        );
    }
}

impl Domain {
    /// Register a memory heap.
    ///
    /// # Safety
    /// The storage described by `descriptor` must remain valid until the
    /// returned [`MemoryHeap`] is dropped.
    #[must_use]
    pub unsafe fn register_memory_heap(
        &self,
        descriptor: &MemoryHeapDescriptor<'_>,
    ) -> MemoryHeap<'_> {
        let handle = match &descriptor.storage {
            HeapStorage::Virtual(range) => {
                let specific = (*range).raw();
                register_heap(
                    self,
                    descriptor,
                    addr_of!(specific).cast(),
                    size_of_val(&specific),
                )
            }
            #[cfg(feature = "memory_cuda_runtime")]
            HeapStorage::CudaArray(array) => register_heap(
                self,
                descriptor,
                addr_of!(*array).cast(),
                size_of_val(array),
            ),
            #[cfg(feature = "memory_cuda_runtime")]
            HeapStorage::CuArray(array) => register_heap(
                self,
                descriptor,
                addr_of!(*array).cast(),
                size_of_val(array),
            ),
        };
        MemoryHeap {
            domain: self,
            handle,
            owned: true,
        }
    }

    /// Return the process-wide virtual-address heap.
    #[must_use]
    pub fn process_wide_memory_heap(&self) -> MemoryHeap<'_> {
        MemoryHeap {
            domain: self,
            handle: nvtx_sys::memory::MemoryHeapHandle::process_wide(),
            owned: false,
        }
    }

    /// Create a memory permissions object.
    #[must_use]
    pub fn create_memory_permissions(
        &self,
        flags: PermissionCreationFlags,
    ) -> MemoryPermissions<'_> {
        MemoryPermissions {
            domain: self,
            handle: nvtx_sys::memory::permissions_create(self.raw_handle(), flags.0 as i32),
            owned: true,
        }
    }

    /// Return the process-wide memory permissions object.
    #[must_use]
    pub fn process_wide_memory_permissions(&self) -> MemoryPermissions<'_> {
        MemoryPermissions {
            domain: self,
            handle: nvtx_sys::memory::MemoryPermissionsHandle::process_wide(),
            owned: false,
        }
    }

    /// Return the CUDA process-wide permissions object.
    #[cfg(feature = "memory_cuda_runtime")]
    #[must_use]
    pub fn cuda_process_wide_memory_permissions(&self) -> MemoryPermissions<'_> {
        MemoryPermissions {
            domain: self,
            handle: nvtx_sys::memory::cuda_process_wide_permissions(self.raw_handle()),
            owned: false,
        }
    }

    /// Return a CUDA device-wide permissions object.
    #[cfg(feature = "memory_cuda_runtime")]
    #[must_use]
    pub fn cuda_device_memory_permissions(&self, device: i32) -> MemoryPermissions<'_> {
        MemoryPermissions {
            domain: self,
            handle: nvtx_sys::memory::cuda_device_wide_permissions(self.raw_handle(), device),
            owned: false,
        }
    }

    /// Mark virtual memory ranges initialized on a CUDA stream.
    ///
    /// # Safety
    /// `stream` must be a valid CUDA Runtime stream and the ranges must remain
    /// valid until the queued CUDA work has consumed the annotation.
    #[cfg(feature = "memory_cuda_runtime")]
    pub unsafe fn mark_cuda_initialized(
        &self,
        stream: nvtx_sys::CudaStream,
        is_per_thread_stream: bool,
        ranges: &[VirtualRange<'_>],
    ) {
        let raw: Vec<_> = ranges.iter().copied().map(VirtualRange::raw).collect();
        let batch = nvtx_sys::memory::MarkInitializedBatch {
            extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
            structSize: size_of::<nvtx_sys::memory::MarkInitializedBatch>() as u16,
            regionType: nvtx_sys::ffi::NVTX_MEM_TYPE_VIRTUAL_ADDRESS as u32,
            regionDescCount: raw.len(),
            regionDescElementSize: size_of::<nvtx_sys::memory::VirtualRangeDescriptor>(),
            regionDescElements: raw.as_ptr().cast(),
        };
        // SAFETY: The caller guarantees stream and queued-memory lifetimes.
        unsafe {
            nvtx_sys::memory::cuda_mark_initialized(
                self.raw_handle(),
                stream,
                is_per_thread_stream,
                &batch,
            );
        }
    }
}

fn register_heap(
    domain: &Domain,
    descriptor: &MemoryHeapDescriptor<'_>,
    specific: *const c_void,
    specific_size: usize,
) -> nvtx_sys::memory::MemoryHeapHandle {
    let (memory_type, message_type, message) = match descriptor.storage {
        HeapStorage::Virtual(_) => (
            nvtx_sys::ffi::NVTX_MEM_TYPE_VIRTUAL_ADDRESS as u32,
            descriptor.name.map_or(
                nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_UNKNOWN as u32,
                |_| nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_TYPE_ASCII as u32,
            ),
            nvtx_sys::ffi::nvtxMessageValue_t {
                ascii: descriptor.name.map_or(core::ptr::null(), CStr::as_ptr),
            },
        ),
        #[cfg(feature = "memory_cuda_runtime")]
        HeapStorage::CudaArray(_) => (
            nvtx_sys::ffi::NVTX_MEM_TYPE_CUDA_ARRAY as u32,
            descriptor.name.map_or(
                nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_UNKNOWN as u32,
                |_| nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_TYPE_ASCII as u32,
            ),
            nvtx_sys::ffi::nvtxMessageValue_t {
                ascii: descriptor.name.map_or(core::ptr::null(), CStr::as_ptr),
            },
        ),
        #[cfg(feature = "memory_cuda_runtime")]
        HeapStorage::CuArray(_) => (
            nvtx_sys::ffi::NVTX_MEM_TYPE_CU_ARRAY as u32,
            descriptor.name.map_or(
                nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_UNKNOWN as u32,
                |_| nvtx_sys::ffi::nvtxMessageType_t::NVTX_MESSAGE_TYPE_ASCII as u32,
            ),
            nvtx_sys::ffi::nvtxMessageValue_t {
                ascii: descriptor.name.map_or(core::ptr::null(), CStr::as_ptr),
            },
        ),
    };
    let raw = nvtx_sys::memory::HeapDescriptor {
        extCompatID: nvtx_sys::ffi::NVTX_EXT_COMPATID_MEM as u16,
        structSize: size_of::<nvtx_sys::memory::HeapDescriptor>() as u16,
        reserved0: 0,
        usage: descriptor.usage.raw(),
        type_: memory_type,
        typeSpecificDescSize: specific_size,
        typeSpecificDesc: specific,
        category: descriptor.category,
        messageType: message_type,
        message,
    };
    // SAFETY: The type-specific descriptor and optional C string remain valid
    // for the complete registration call.
    unsafe { nvtx_sys::memory::heap_register(domain.raw_handle(), &raw) }
}

// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use nvtx::{
    Domain, MemoryError, MemoryHeapDescriptor, MemoryHeapUsage, PermissionAccess,
    PermissionAssignment, PermissionBindFlags, PermissionBindScope, PermissionCreationFlags,
    VirtualRange,
};

fn main() -> Result<(), MemoryError> {
    let domain = Domain::new(c"allocator");
    let storage = [0_u8; 4096];
    let range = VirtualRange::from_slice(&storage);
    let descriptor =
        MemoryHeapDescriptor::virtual_address(MemoryHeapUsage::Layout, range).name(c"arena");
    // SAFETY: `storage` outlives the heap and all regions registered against it.
    let heap = unsafe { domain.register_memory_heap(&descriptor) };
    let regions = heap.register_virtual_regions(&[range])?;
    let region = regions.region(0).ok_or(MemoryError::RegionOutOfBounds)?;

    let permissions =
        domain.create_memory_permissions(PermissionCreationFlags::new().exclude_global_write());
    permissions.assign(&[PermissionAssignment::new(
        region,
        PermissionAccess::none().read().write(),
    )])?;
    let _binding = permissions.bind(
        PermissionBindScope::CpuThread,
        PermissionBindFlags::new().strict_write(),
    );
    Ok(())
}

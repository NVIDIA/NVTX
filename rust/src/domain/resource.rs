// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use core::marker::PhantomData;

/// Named resource handle
pub struct Resource<'a> {
    pub(super) handle: nvtx_sys::ResourceHandle,
    pub(super) _lifetime: PhantomData<&'a ()>,
}

impl Drop for Resource<'_> {
    fn drop(&mut self) {
        nvtx_sys::domain_resource_destroy(self.handle);
    }
}

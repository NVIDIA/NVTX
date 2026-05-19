// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#[cfg(feature = "cuda")]
pub use nvtx_sys::{CuContext, CuDevice, CuEvent, CuStream};

#[cfg(feature = "cuda_runtime")]
pub use nvtx_sys::{CudaEvent, CudaStream};

#[cfg(target_family = "unix")]
pub use libc::{pthread_cond_t, pthread_mutex_t, pthread_once_t, pthread_rwlock_t};

#[cfg(all(target_family = "unix", not(target_os = "macos")))]
pub use libc::{pthread_barrier_t, pthread_spinlock_t};

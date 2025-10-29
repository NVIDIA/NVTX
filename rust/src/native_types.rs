#[cfg(feature = "cuda")]
pub use nvtx_sys::{CuContext, CuDevice, CuEvent, CuStream};

#[cfg(feature = "cuda_runtime")]
pub use nvtx_sys::{CudaEvent, CudaStream};

#[cfg(target_family = "unix")]
pub use libc::{
    pthread_barrier_t, pthread_cond_t, pthread_mutex_t, pthread_once_t, pthread_rwlock_t,
    pthread_spinlock_t,
};

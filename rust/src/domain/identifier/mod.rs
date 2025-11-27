use crate::TypeValueEncodable;

mod generic;
pub use generic::GenericIdentifier;

#[cfg(target_family = "unix")]
mod pthread;
#[cfg(target_family = "unix")]
pub use pthread::PThreadIdentifier;

#[cfg(feature = "cuda_runtime")]
mod cuda_runtime;
#[cfg(feature = "cuda_runtime")]
pub use cuda_runtime::CudaRuntimeIdentifier;

#[cfg(feature = "cuda")]
mod cuda;
#[cfg(feature = "cuda")]
pub use cuda::CudaIdentifier;

/// Identifier used for supported resource types
#[non_exhaustive]
pub enum Identifier {
    /// Generic identifier
    Generic(GenericIdentifier),
    /// PThread-specific identifier
    #[cfg(target_family = "unix")]
    PThread(PThreadIdentifier),
    /// CUDA specific identifier
    #[cfg(feature = "cuda")]
    Cuda(CudaIdentifier),
    /// CUDA runtime specific identifier
    #[cfg(feature = "cuda_runtime")]
    CudaRuntime(CudaRuntimeIdentifier),
}

impl TypeValueEncodable for Identifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        match self {
            Identifier::Generic(g) => g.encode(),
            #[cfg(target_family = "unix")]
            Identifier::PThread(p) => p.encode(),
            #[cfg(feature = "cuda")]
            Identifier::Cuda(c) => c.encode(),
            #[cfg(feature = "cuda_runtime")]
            Identifier::CudaRuntime(c) => c.encode(),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        (
            nvtx_sys::resource_type::UNKNOWN,
            Self::Value { ullValue: 0 },
        )
    }
}

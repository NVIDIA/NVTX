use super::Identifier;
use crate::TypeValueEncodable;

/// Identifiers used for CUDA resources
pub enum CudaIdentifier {
    /// Device
    Device(nvtx_sys::CuDevice),
    /// Context
    Context(nvtx_sys::CuContext),
    /// Event
    Event(nvtx_sys::CuEvent),
    /// Stream
    Stream(nvtx_sys::CuStream),
}

impl From<CudaIdentifier> for Identifier {
    fn from(value: CudaIdentifier) -> Self {
        Self::Cuda(value)
    }
}

impl TypeValueEncodable for CudaIdentifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        match self {
            Self::Device(id) => (
                nvtx_sys::resource_type::CUDA_DEVICE,
                Self::Value {
                    ullValue: *id as u64,
                },
            ),
            Self::Context(id) => (
                nvtx_sys::resource_type::CUDA_CONTEXT,
                Self::Value { pValue: id.cast() },
            ),
            Self::Event(id) => (
                nvtx_sys::resource_type::CUDA_EVENT,
                Self::Value { pValue: id.cast() },
            ),
            Self::Stream(id) => (
                nvtx_sys::resource_type::CUDA_STREAM,
                Self::Value { pValue: id.cast() },
            ),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        Identifier::default_encoding()
    }
}

#[cfg(test)]
mod tests {

    use std::os::raw::c_void;

    use super::*;

    #[test]
    fn test_identifier_device() {
        let device_id = 0;
        let x = CudaIdentifier::Device(device_id);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Cuda(CudaIdentifier::Device(id)) if id == device_id));
    }

    #[test]
    fn test_identifier_context() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CuContext;
        let x = CudaIdentifier::Context(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Cuda(CudaIdentifier::Context(p)) if p == ptr));
    }

    #[test]
    fn test_identifier_event() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CuEvent;
        let x = CudaIdentifier::Event(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Cuda(CudaIdentifier::Event(p)) if p == ptr));
    }

    #[test]
    fn test_identifier_stream() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CuStream;
        let x = CudaIdentifier::Stream(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Cuda(CudaIdentifier::Stream(p)) if p == ptr));
    }

    #[test]
    fn test_encode_device() {
        let device_id: nvtx_sys::CuDevice = 0;
        let x = CudaIdentifier::Device(device_id);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDA_DEVICE);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { ullValue: id } if id == (device_id as u64))
            );
        }
    }

    #[test]
    fn test_encode_context() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CuContext;
        let x = CudaIdentifier::Context(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDA_CONTEXT);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_event() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CuEvent;
        let x = CudaIdentifier::Event(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDA_EVENT);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_stream() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CuStream;
        let x = CudaIdentifier::Stream(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDA_STREAM);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }
}

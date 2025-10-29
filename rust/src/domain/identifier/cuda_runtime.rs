use super::Identifier;
use crate::TypeValueEncodable;

/// Identifiers used for CUDA resources
pub enum CudaRuntimeIdentifier {
    /// Device
    Device(i32),
    /// Event
    Event(nvtx_sys::CudaEvent),
    /// Stream
    Stream(nvtx_sys::CudaStream),
}

impl From<CudaRuntimeIdentifier> for Identifier {
    fn from(value: CudaRuntimeIdentifier) -> Self {
        Self::CudaRuntime(value)
    }
}

impl TypeValueEncodable for CudaRuntimeIdentifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        use nvtx_sys::resource_type::*;
        match self {
            Self::Device(id) => (
                CUDART_DEVICE,
                Self::Value {
                    ullValue: *id as u64,
                },
            ),
            Self::Event(id) => (CUDART_EVENT, Self::Value { pValue: id.cast() }),
            Self::Stream(id) => (CUDART_STREAM, Self::Value { pValue: id.cast() }),
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
        let x = CudaRuntimeIdentifier::Device(device_id);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::CudaRuntime(CudaRuntimeIdentifier::Device(id)) if id == device_id)
        );
    }

    #[test]
    fn test_identifier_event() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CudaEvent;
        let x = CudaRuntimeIdentifier::Event(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::CudaRuntime(CudaRuntimeIdentifier::Event(p)) if p == ptr));
    }

    #[test]
    fn test_identifier_stream() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CudaStream;
        let x = CudaRuntimeIdentifier::Stream(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::CudaRuntime(CudaRuntimeIdentifier::Stream(p)) if p == ptr));
    }

    #[test]
    fn test_encode_device() {
        let device_id = 0;
        let x = CudaRuntimeIdentifier::Device(device_id);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDART_DEVICE);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { ullValue: id } if id == (device_id as u64))
            );
        }
    }

    #[test]
    fn test_encode_event() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CudaEvent;
        let x = CudaRuntimeIdentifier::Event(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDART_EVENT);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_stream() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as nvtx_sys::CudaStream;
        let x = CudaRuntimeIdentifier::Stream(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::CUDART_STREAM);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }
}

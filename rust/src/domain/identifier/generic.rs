use super::Identifier;
use crate::TypeValueEncodable;

/// Identifiers used for Generic resources
pub enum GenericIdentifier {
    /// Generic pointer
    Pointer(*const ::std::os::raw::c_void),
    /// Generic handle
    Handle(u64),
    /// Generic thread native
    NativeThread(u64),
    /// Generic thread posix
    PosixThread(u64),
}

impl From<GenericIdentifier> for Identifier {
    fn from(value: GenericIdentifier) -> Self {
        Self::Generic(value)
    }
}

impl TypeValueEncodable for GenericIdentifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        match self {
            Self::Pointer(p) => (
                nvtx_sys::resource_type::GENERIC_POINTER,
                Self::Value { pValue: *p },
            ),
            Self::Handle(h) => (
                nvtx_sys::resource_type::GENERIC_HANDLE,
                Self::Value { ullValue: *h },
            ),
            Self::NativeThread(t) => (
                nvtx_sys::resource_type::GENERIC_THREAD_NATIVE,
                Self::Value { ullValue: *t },
            ),
            Self::PosixThread(t) => (
                nvtx_sys::resource_type::GENERIC_THREAD_POSIX,
                Self::Value { ullValue: *t },
            ),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        Identifier::default_encoding()
    }
}

#[cfg(test)]
mod tests {

    use super::*;

    #[test]
    fn test_identifier_pointer() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const std::os::raw::c_void;
        let x = GenericIdentifier::Pointer(ptr);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Generic(GenericIdentifier::Pointer(p)) if p == ptr));
    }

    #[test]
    fn test_identifier_handle() {
        let val = 0xDEADBEEF01234567_u64;
        let x = GenericIdentifier::Handle(val);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Generic(GenericIdentifier::Handle(v)) if v == val));
    }

    #[test]
    fn test_identifier_native_thread() {
        let val = 0xDEADBEEF01234567_u64;
        let x = GenericIdentifier::NativeThread(val);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Generic(GenericIdentifier::NativeThread(v)) if v == val));
    }

    #[test]
    fn test_identifier_posix_thread() {
        let val = 0xDEADBEEF01234567_u64;
        let x = GenericIdentifier::PosixThread(val);
        let i = Identifier::from(x);
        assert!(matches!(i, Identifier::Generic(GenericIdentifier::PosixThread(v)) if v == val));
    }

    #[test]
    fn test_encode_pointer() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const std::os::raw::c_void;
        let x = GenericIdentifier::Pointer(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::GENERIC_POINTER);
        unsafe {
            assert!(matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if p == ptr));
        }
    }

    #[test]
    fn test_encode_handle() {
        let val = 0xDEADBEEF01234567_u64;
        let x = GenericIdentifier::Handle(val);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::GENERIC_HANDLE);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { ullValue: v } if v == val)
            );
        }
    }

    #[test]
    fn test_encode_native_thread() {
        let val = 0xDEADBEEF01234567_u64;
        let x = GenericIdentifier::NativeThread(val);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::GENERIC_THREAD_NATIVE);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { ullValue: v } if v == val)
            );
        }
    }

    #[test]
    fn test_encode_posix_thread() {
        let val = 0xDEADBEEF01234567_u64;
        let x = GenericIdentifier::PosixThread(val);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::GENERIC_THREAD_POSIX);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { ullValue: v } if v == val)
            );
        }
    }
}

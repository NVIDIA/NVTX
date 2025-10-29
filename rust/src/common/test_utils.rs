#[cfg(test)]
use crate::{common::CategoryEncodable, Color, Payload, TypeValueEncodable};
use widestring::{WideCStr, WideCString};

/// Utility struct for common test assertions and helpers.
pub struct TestUtils;

#[cfg(test)]
impl TestUtils {
    /// Assert that a message encodes to ASCII correctly
    pub fn assert_message_ascii_encoding<T>(message: &T, expected_str: &str)
    where
        T: TypeValueEncodable<Type = nvtx_sys::MessageType, Value = nvtx_sys::MessageValue>,
    {
        let (t, v) = message.encode();
        assert_eq!(t, nvtx_sys::MessageType::NVTX_MESSAGE_TYPE_ASCII);
        let expected_cstr = std::ffi::CString::new(expected_str).unwrap();
        unsafe {
            assert!(
                matches!(v, nvtx_sys::MessageValue{ ascii: p } if std::ffi::CStr::from_ptr(p) == expected_cstr.as_c_str())
            );
        }
    }

    /// Assert that a message encodes to Unicode correctly
    pub fn assert_message_unicode_encoding<T>(message: &T, expected_str: &str)
    where
        T: TypeValueEncodable<Type = nvtx_sys::MessageType, Value = nvtx_sys::MessageValue>,
    {
        let (t, v) = message.encode();
        assert_eq!(t, nvtx_sys::MessageType::NVTX_MESSAGE_TYPE_UNICODE);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::MessageValue{ unicode: p } if WideCStr::from_ptr_str(p.cast()) == WideCString::from_str(expected_str).unwrap())
            );
        }
    }

    /// Assert that a color encodes correctly
    pub fn assert_color_encoding(
        color: &Color,
        expected_r: u8,
        expected_g: u8,
        expected_b: u8,
        expected_a: u8,
    ) {
        let (t, v) = color.encode();
        assert_eq!(t, nvtx_sys::ColorType::NVTX_COLOR_ARGB);
        assert_eq!(
            v,
            u32::from_be_bytes([expected_a, expected_r, expected_g, expected_b])
        );
    }

    /// Assert that a category encodes correctly
    pub fn assert_category_encoding<T>(category: &T, expected_id: u32)
    where
        T: CategoryEncodable,
    {
        let encoded_id = category.encode_id();
        assert_eq!(encoded_id, expected_id);
    }

    /// Assert that domain string registration works correctly
    pub fn assert_domain_string_registration(domain: &crate::Domain, name: &str) {
        let registered = domain.register_string(name);
        let registered2 = domain.register_string(name);
        assert_eq!(registered, registered2); // Should be cached
    }

    /// Assert that domain category registration works correctly
    pub fn assert_domain_category_registration(domain: &crate::Domain, name: &str) {
        let registered = domain.register_category(name);
        let registered2 = domain.register_category(name);
        assert_eq!(registered, registered2); // Should be cached
    }

    /// Assert that a payload is an i32 and matches the expected value.
    pub fn assert_payload_i32(payload: &Payload, expected: i32) {
        match payload {
            Payload::Int32(val) => assert_eq!(*val, expected),
            _ => panic!("Expected Payload::Int32"),
        }
    }
    /// Assert that a payload is a u32 and matches the expected value.
    pub fn assert_payload_u32(payload: &Payload, expected: u32) {
        match payload {
            Payload::Uint32(val) => assert_eq!(*val, expected),
            _ => panic!("Expected Payload::Uint32"),
        }
    }
    /// Assert that a payload is an i64 and matches the expected value.
    pub fn assert_payload_i64(payload: &Payload, expected: i64) {
        match payload {
            Payload::Int64(val) => assert_eq!(*val, expected),
            _ => panic!("Expected Payload::Int64"),
        }
    }
    /// Assert that a payload is a u64 and matches the expected value.
    pub fn assert_payload_u64(payload: &Payload, expected: u64) {
        match payload {
            Payload::Uint64(val) => assert_eq!(*val, expected),
            _ => panic!("Expected Payload::Uint64"),
        }
    }
    /// Assert that a payload is a f32 and matches the expected value.
    pub fn assert_payload_f32(payload: &Payload, expected: f32) {
        match payload {
            Payload::Float(val) => assert!((*val - expected).abs() < f32::EPSILON),
            _ => panic!("Expected Payload::Float"),
        }
    }
    /// Assert that a payload is a f64 and matches the expected value.
    pub fn assert_payload_f64(payload: &Payload, expected: f64) {
        match payload {
            Payload::Double(val) => assert!((*val - expected).abs() < f64::EPSILON),
            _ => panic!("Expected Payload::Double"),
        }
    }
}

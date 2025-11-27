use crate::TypeValueEncodable;

/// Represents a payload value for use within [`crate::EventAttributes`] and
/// [`crate::domain::EventAttributes`].
///
/// * [`Payload::Float`] holds a 32-bit floating-point playload
/// * [`Payload::Double`] holds a 64-bit floating-point playload
/// * [`Payload::Int32`] holds a 32-bit integral playload
/// * [`Payload::Int64`] holds a 64-bit integral playload
/// * [`Payload::Uint32`] holds a 32-bit unsigned integral playload
/// * [`Payload::Uint64`] holds a 64-bit unsigned integral playload
#[derive(Debug, Clone, Copy)]
pub enum Payload {
    /// A 32-bit floating-point value.
    Float(f32),
    /// A 64-bit floating-point value.
    Double(f64),
    /// A 32-bit integral value.
    Int32(i32),
    /// A 64-bit integral value.
    Int64(i64),
    /// A 32-bit unsigned integral value.
    Uint32(u32),
    /// A 64-bit unsigned integral value.
    Uint64(u64),
}

impl From<u64> for Payload {
    fn from(v: u64) -> Self {
        Self::Uint64(v)
    }
}

impl From<u32> for Payload {
    fn from(v: u32) -> Self {
        Self::Uint32(v)
    }
}

impl From<i64> for Payload {
    fn from(v: i64) -> Self {
        Self::Int64(v)
    }
}

impl From<i32> for Payload {
    fn from(v: i32) -> Self {
        Self::Int32(v)
    }
}

impl From<f64> for Payload {
    fn from(v: f64) -> Self {
        Self::Double(v)
    }
}

impl From<f32> for Payload {
    fn from(v: f32) -> Self {
        Self::Float(v)
    }
}

impl TypeValueEncodable for Payload {
    type Type = nvtx_sys::PayloadType;
    type Value = nvtx_sys::PayloadValue;
    fn encode(&self) -> (Self::Type, Self::Value) {
        match self {
            Payload::Float(x) => (
                Self::Type::NVTX_PAYLOAD_TYPE_FLOAT,
                Self::Value { fValue: *x },
            ),
            Payload::Double(x) => (
                Self::Type::NVTX_PAYLOAD_TYPE_DOUBLE,
                Self::Value { dValue: *x },
            ),
            Payload::Int32(x) => (
                Self::Type::NVTX_PAYLOAD_TYPE_INT32,
                Self::Value { iValue: *x },
            ),
            Payload::Int64(x) => (
                Self::Type::NVTX_PAYLOAD_TYPE_INT64,
                Self::Value { llValue: *x },
            ),
            Payload::Uint32(x) => (
                Self::Type::NVTX_PAYLOAD_TYPE_UNSIGNED_INT32,
                Self::Value { uiValue: *x },
            ),
            Payload::Uint64(x) => (
                Self::Type::NVTX_PAYLOAD_TYPE_UNSIGNED_INT64,
                Self::Value { ullValue: *x },
            ),
        }
    }

    fn default_encoding() -> (Self::Type, Self::Value) {
        (
            Self::Type::NVTX_PAYLOAD_UNKNOWN,
            Self::Value { ullValue: 0 },
        )
    }
}

#[cfg(test)]
mod tests {
    use super::Payload;
    use crate::common::TestUtils;

    #[test]
    fn test_payload_int32() {
        let payload = Payload::Int32(i32::MAX);
        TestUtils::assert_payload_i32(&payload, i32::MAX);
    }
    #[test]
    fn test_payload_uint32() {
        let payload = Payload::Uint32(u32::MAX);
        TestUtils::assert_payload_u32(&payload, u32::MAX);
    }
    #[test]
    fn test_payload_int64() {
        let payload = Payload::Int64(i64::MAX);
        TestUtils::assert_payload_i64(&payload, i64::MAX);
    }
    #[test]
    fn test_payload_uint64() {
        let payload = Payload::Uint64(u64::MAX);
        TestUtils::assert_payload_u64(&payload, u64::MAX);
    }
    #[test]
    fn test_payload_float() {
        let payload = Payload::Float(std::f32::consts::PI);
        TestUtils::assert_payload_f32(&payload, std::f32::consts::PI);
    }
    #[test]
    fn test_payload_double() {
        let payload = Payload::Double(std::f64::consts::E);
        TestUtils::assert_payload_f64(&payload, std::f64::consts::E);
    }
}

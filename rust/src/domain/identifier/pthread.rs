use super::Identifier;
use crate::{
    native_types::{
        pthread_barrier_t, pthread_cond_t, pthread_mutex_t, pthread_once_t, pthread_rwlock_t,
        pthread_spinlock_t,
    },
    TypeValueEncodable,
};

/// Identifiers used for PThread resources
pub enum PThreadIdentifier {
    /// PThread mutex
    Mutex(*const pthread_mutex_t),
    /// PThread condition_variable
    Condition(*const pthread_cond_t),
    /// PThread rwlock
    RWLock(*const pthread_rwlock_t),
    /// PThread barrier
    Barrier(*const pthread_barrier_t),
    /// PThread spinlock
    Spinlock(*const pthread_spinlock_t),
    /// PThread once
    Once(*const pthread_once_t),
}

impl From<PThreadIdentifier> for Identifier {
    fn from(value: PThreadIdentifier) -> Self {
        Self::PThread(value)
    }
}

impl TypeValueEncodable for PThreadIdentifier {
    type Type = u32;
    type Value = nvtx_sys::ResourceAttributesIdentifier;

    fn encode(&self) -> (Self::Type, Self::Value) {
        match self {
            Self::Mutex(m) => (
                nvtx_sys::resource_type::PTHREAD_MUTEX,
                Self::Value { pValue: m.cast() },
            ),
            Self::Condition(cv) => (
                nvtx_sys::resource_type::PTHREAD_CONDITION,
                Self::Value { pValue: cv.cast() },
            ),
            Self::RWLock(rwl) => (
                nvtx_sys::resource_type::PTHREAD_RWLOCK,
                Self::Value { pValue: rwl.cast() },
            ),
            Self::Barrier(bar) => (
                nvtx_sys::resource_type::PTHREAD_BARRIER,
                Self::Value { pValue: bar.cast() },
            ),
            Self::Spinlock(s) => (
                nvtx_sys::resource_type::PTHREAD_SPINLOCK,
                Self::Value { pValue: s.cast() },
            ),
            Self::Once(o) => (
                nvtx_sys::resource_type::PTHREAD_ONCE,
                Self::Value { pValue: o.cast() },
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
    fn test_identifier_mutex() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_mutex_t;
        let x = PThreadIdentifier::Mutex(ptr);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::PThread(PThreadIdentifier::Mutex(p)) if std::ptr::eq(p, ptr))
        );
    }

    #[test]
    fn test_identifier_cv() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_cond_t;
        let x = PThreadIdentifier::Condition(ptr);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::PThread(PThreadIdentifier::Condition(p)) if std::ptr::eq(p, ptr))
        );
    }

    #[test]
    fn test_identifier_barrier() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_barrier_t;
        let x = PThreadIdentifier::Barrier(ptr);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::PThread(PThreadIdentifier::Barrier(p)) if std::ptr::eq(p, ptr))
        );
    }

    #[test]
    fn test_identifier_rwlock() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_rwlock_t;
        let x = PThreadIdentifier::RWLock(ptr);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::PThread(PThreadIdentifier::RWLock(p)) if std::ptr::eq(p, ptr))
        );
    }

    #[test]
    fn test_identifier_spinlock() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_spinlock_t;
        let x = PThreadIdentifier::Spinlock(ptr);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::PThread(PThreadIdentifier::Spinlock(p)) if std::ptr::eq(p, ptr))
        );
    }

    #[test]
    fn test_identifier_once() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_once_t;
        let x = PThreadIdentifier::Once(ptr);
        let i = Identifier::from(x);
        assert!(
            matches!(i, Identifier::PThread(PThreadIdentifier::Once(p)) if std::ptr::eq(p, ptr))
        );
    }

    #[test]
    fn test_encode_mutex() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_mutex_t;
        let x = PThreadIdentifier::Mutex(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::PTHREAD_MUTEX);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_cv() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_cond_t;
        let x = PThreadIdentifier::Condition(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::PTHREAD_CONDITION);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_barrier() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_barrier_t;
        let x = PThreadIdentifier::Barrier(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::PTHREAD_BARRIER);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_rwlock() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_rwlock_t;
        let x = PThreadIdentifier::RWLock(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::PTHREAD_RWLOCK);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_spinlock() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_spinlock_t;
        let x = PThreadIdentifier::Spinlock(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::PTHREAD_SPINLOCK);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }

    #[test]
    fn test_encode_once() {
        let dummy = ();
        let ptr = std::ptr::addr_of!(dummy) as *const pthread_once_t;
        let x = PThreadIdentifier::Once(ptr);
        let (t, v) = x.encode();
        assert_eq!(t, nvtx_sys::resource_type::PTHREAD_ONCE);
        unsafe {
            assert!(
                matches!(v, nvtx_sys::ResourceAttributesIdentifier { pValue: p } if std::ptr::eq(p, ptr as *const c_void))
            );
        }
    }
}

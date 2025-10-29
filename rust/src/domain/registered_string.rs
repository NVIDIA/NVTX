use crate::Domain;

/// Handle for retrieving a registered string.
///
/// See [`Domain::register_string`] and [`Domain::register_strings`]
#[derive(Debug, Clone, Copy)]
pub struct RegisteredString<'a> {
    handle: nvtx_sys::StringHandle,
    uid: u32,
    domain: &'a Domain,
}

impl PartialEq for RegisteredString<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.handle == other.handle
            && self.uid == other.uid
            && std::ptr::eq(self.domain, other.domain)
    }
}

impl Eq for RegisteredString<'_> {}

impl<'a> RegisteredString<'a> {
    pub(super) fn new(
        handle: nvtx_sys::StringHandle,
        uid: u32,
        domain: &'a Domain,
    ) -> RegisteredString<'a> {
        RegisteredString {
            handle,
            uid,
            domain,
        }
    }

    pub(crate) fn handle(&self) -> nvtx_sys::StringHandle {
        self.handle
    }

    pub(crate) fn domain(&self) -> &Domain {
        self.domain
    }
}

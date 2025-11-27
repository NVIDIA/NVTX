use crate::{common::CategoryEncodable, Domain};

/// Represents a domain-owned category for use with mark and range grouping.
///
/// See [`Domain::register_category`], [`Domain::register_categories`]
#[derive(Debug, Clone, Copy)]
pub struct Category<'a> {
    id: u32,
    domain: &'a Domain,
}

impl PartialEq for Category<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id && std::ptr::eq(self.domain, other.domain)
    }
}

impl Eq for Category<'_> {}

impl<'a> Category<'a> {
    pub(super) fn new(id: u32, domain: &'a Domain) -> Category<'a> {
        Category { id, domain }
    }

    pub(super) fn domain(&self) -> &Domain {
        self.domain
    }
}

impl CategoryEncodable for Category<'_> {
    fn encode_id(&self) -> u32 {
        self.id
    }
}

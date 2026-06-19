// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::{common::CategoryEncodable, Str};
use core::sync::atomic::{AtomicU32, Ordering};

/// Represents a category for use with mark and range grouping.
///
/// Categories can be created via:
/// * [`crate::register_category`]
/// * [`crate::register_categories`]
/// * [`Category::new`]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Category {
    pub(super) id: u32,
}

impl Category {
    /// Create a new category not affiliated with any domain.
    ///
    /// See [`Str`] for valid conversions
    pub fn new(name: impl Into<Str>) -> Category {
        static COUNT: AtomicU32 = AtomicU32::new(0);
        let id: u32 = 1 + COUNT.fetch_add(1, Ordering::SeqCst);
        match name.into() {
            Str::Ascii(s) => nvtx_sys::name_category_ascii(id, &s),
            Str::Unicode(s) => nvtx_sys::name_category_unicode(id, &s),
        }
        Category { id }
    }
}

impl CategoryEncodable for Category {
    fn encode_id(&self) -> u32 {
        self.id
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use super::*;
    use crate::common::TestUtils;

    fn lossy_str(value: &str) -> Str {
        Str::from_str_lossy(value)
    }

    #[test]
    fn test_unique_categories() {
        let cat1 = Category::new(lossy_str("category 1"));
        let cat2 = Category::new(lossy_str("category 1"));
        assert_ne!(cat1, cat2);
    }

    #[test]
    fn test_category_encoding() {
        let cat = Category::new(lossy_str("test category"));
        TestUtils::assert_category_encoding(&cat, cat.id);
    }
}

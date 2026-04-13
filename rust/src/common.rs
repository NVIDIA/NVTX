// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

pub mod event_argument;
pub mod event_attributes;
pub mod message;
/// Test utilities for internal and integration tests.
#[cfg(all(test, feature = "std"))]
pub mod test_utils;

// Re-export commonly used items for convenience
pub use event_argument::GenericEventArgument;
pub(crate) use event_attributes::CategoryEncodable;
pub use event_attributes::{GenericEventAttributes, GenericEventAttributesBuilder};
pub use message::GenericMessage;
#[cfg(all(test, feature = "std"))]
pub use test_utils::TestUtils;

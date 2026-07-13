// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use core::marker::PhantomData;

use super::EventArgument;
use crate::Domain;

/// A RAII-like object for modeling process-wide Ranges within a Domain.
#[derive(Debug)]
pub struct Range<'a> {
    pub(super) id: Option<nvtx_sys::RangeId>,
    pub(super) domain: &'a Domain,
}

impl<'a> Range<'a> {
    pub(super) fn new(arg: impl Into<EventArgument<'a>>, domain: &'a Domain) -> Range<'a> {
        Range::new_from_arg_with_domain(arg, domain)
    }

    fn new_from_arg_with_domain(arg: impl Into<EventArgument<'a>>, domain: &'a Domain) -> Self {
        Range {
            id: Some(domain.range_start(arg)),
            domain,
        }
    }
}

impl Drop for Range<'_> {
    fn drop(&mut self) {
        if let Some(id) = self.id {
            self.domain.range_end(id);
        }
    }
}

/// A RAII-like object for modeling callstack (thread-local) Ranges within a Domain.
#[derive(Debug)]
pub struct LocalRange<'a> {
    pub(super) domain: &'a Domain,
    pub(super) active: bool,
    // prevent Sync + Send
    _phantom: PhantomData<*mut i32>,
}

impl<'a> LocalRange<'a> {
    pub(super) fn new(arg: impl Into<EventArgument<'a>>, domain: &'a Domain) -> LocalRange<'a> {
        let arg = match arg.into() {
            EventArgument::Attributes(attr) => attr,
            EventArgument::Message(m) => domain.event_attributes_builder().message(m).build(),
        };
        nvtx_sys::domain_range_push_ex(domain.handle, &arg.encode());
        LocalRange {
            domain,
            active: true,
            _phantom: PhantomData,
        }
    }
}

impl Drop for LocalRange<'_> {
    fn drop(&mut self) {
        if self.active {
            nvtx_sys::domain_range_pop(self.domain.handle);
        }
    }
}

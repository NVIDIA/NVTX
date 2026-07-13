// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::common::event_attributes::GenericEventAttributes;
use crate::common::GenericMessage;

/// Generic event argument type that can be used in both global and domain contexts
#[derive(Debug, Clone)]
pub enum GenericEventArgument<M, A> {
    /// Holds a Message.
    Message(M),
    /// Holds an `EventAttributes`.
    Attributes(A),
}

impl<C, M> From<GenericEventAttributes<C, M>>
    for GenericEventArgument<M, GenericEventAttributes<C, M>>
{
    fn from(value: GenericEventAttributes<C, M>) -> Self {
        match value {
            GenericEventAttributes {
                category: None,
                color: None,
                message: Some(m),
                payload: None,
            } => GenericEventArgument::Message(m),
            attr => GenericEventArgument::Attributes(attr),
        }
    }
}

impl<C, R, T: Into<GenericMessage<R>>> From<T>
    for GenericEventArgument<GenericMessage<R>, GenericEventAttributes<C, GenericMessage<R>>>
{
    fn from(value: T) -> Self {
        GenericEventArgument::Message(value.into())
    }
}

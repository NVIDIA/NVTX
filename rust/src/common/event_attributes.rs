// SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use crate::{Color, Payload, TypeValueEncodable};

/// Generic event attributes that can be used in both global and domain contexts.
///
/// This struct holds optional fields for category, color, message, and payload.
/// It is used to describe the attributes of an NVTX event.
#[derive(Debug, Clone)]
pub struct GenericEventAttributes<C, M> {
    pub category: Option<C>,
    pub color: Option<Color>,
    pub message: Option<M>,
    pub payload: Option<Payload>,
}

/// Builder for [`GenericEventAttributes`].
#[derive(Debug, Clone)]
pub struct GenericEventAttributesBuilder<C, M> {
    category: Option<C>,
    color: Option<Color>,
    message: Option<M>,
    payload: Option<Payload>,
}

impl<C, M> Default for GenericEventAttributesBuilder<C, M> {
    fn default() -> Self {
        Self {
            category: None,
            color: None,
            message: None,
            payload: None,
        }
    }
}

impl<C, M> GenericEventAttributesBuilder<C, M> {
    /// Set the event category.
    #[must_use]
    pub fn category(mut self, category: impl Into<C>) -> Self {
        self.category = Some(category.into());
        self
    }

    /// Set the event color.
    #[must_use]
    pub fn color(mut self, color: impl Into<Color>) -> Self {
        self.color = Some(color.into());
        self
    }

    /// Set the event message.
    #[must_use]
    pub fn message(mut self, message: impl Into<M>) -> Self {
        self.message = Some(message.into());
        self
    }

    /// Clear the event message.
    #[must_use]
    pub(crate) fn clear_message(mut self) -> Self {
        self.message = None;
        self
    }

    /// Set the event payload.
    #[must_use]
    pub fn payload(mut self, payload: impl Into<Payload>) -> Self {
        self.payload = Some(payload.into());
        self
    }

    /// Build the event attributes, allowing all fields to be optional.
    pub fn build(self) -> GenericEventAttributes<C, M> {
        GenericEventAttributes {
            category: self.category,
            color: self.color,
            message: self.message,
            payload: self.payload,
        }
    }
}

/// Trait for encoding category IDs.
///
/// Used to abstract over global and domain category types.
pub trait CategoryEncodable {
    /// Encode the category as a u32 ID for NVTX.
    fn encode_id(&self) -> u32;
}

impl<C, M> GenericEventAttributes<C, M>
where
    C: CategoryEncodable,
    M: TypeValueEncodable<Type = nvtx_sys::MessageType, Value = nvtx_sys::MessageValue>,
{
    pub fn encode(&self) -> nvtx_sys::EventAttributes {
        let (color_type, color_value) = self
            .color
            .as_ref()
            .map_or(Color::default_encoding(), Color::encode);
        let (payload_type, payload_value) = self
            .payload
            .as_ref()
            .map_or(Payload::default_encoding(), Payload::encode);
        let cat = self
            .category
            .as_ref()
            .map_or(0, CategoryEncodable::encode_id);
        let (message_type, message_value) = self
            .message
            .as_ref()
            .map_or(M::default_encoding(), M::encode);
        nvtx_sys::EventAttributes {
            // CAST: NVTX_VERSION is forwarded as the raw 16-bit API version field.
            version: nvtx_sys::NVTX_VERSION as u16,
            // CAST: Size is a fixed ABI field encoded as a 16-bit value.
            size: nvtx_sys::NVTX_EVENT_ATTRIBUTES_SIZE as u16,
            category: cat,
            colorType: i32::from(color_type),
            color: color_value,
            payloadType: i32::from(payload_type),
            reserved0: 0,
            payload: payload_value,
            messageType: i32::from(message_type),
            message: message_value,
        }
    }
}

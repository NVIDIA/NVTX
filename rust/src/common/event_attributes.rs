use crate::{common::GenericMessage, Color, Payload, TypeValueEncodable};
use derive_builder::Builder;

/// Generic event attributes that can be used in both global and domain contexts.
///
/// This struct holds optional fields for category, color, message, and payload.
/// It is used to describe the attributes of an NVTX event.
#[derive(Default, Debug, Clone, Builder)]
#[builder(pattern = "owned", derive(Clone), build_fn(skip))]
pub struct GenericEventAttributes<C, M> {
    #[builder(setter(into, strip_option))]
    pub category: Option<C>,
    #[builder(setter(into, strip_option))]
    pub color: Option<Color>,
    #[builder(setter(into, strip_option))]
    pub message: Option<M>,
    #[builder(setter(into, strip_option))]
    pub payload: Option<Payload>,
}

impl<M, T: Into<GenericMessage<M>>, C> From<T> for GenericEventAttributes<C, GenericMessage<M>>
where
    C: CategoryEncodable,
{
    fn from(value: T) -> Self {
        GenericEventAttributes {
            category: None,
            color: None,
            message: Some(value.into()),
            payload: None,
        }
    }
}

impl<C, M> GenericEventAttributesBuilder<C, M> {
    /// Build the event attributes, allowing all fields to be optional.
    pub fn build(self) -> GenericEventAttributes<C, M> {
        GenericEventAttributes {
            category: self.category.flatten(),
            color: self.color.flatten(),
            message: self.message.flatten(),
            payload: self.payload.flatten(),
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
            version: nvtx_sys::NVTX_VERSION as u16,
            size: nvtx_sys::NVTX_EVENT_ATTRIBUTES_SIZE as u16,
            category: cat,
            colorType: color_type as i32,
            color: color_value,
            payloadType: payload_type as i32,
            reserved0: 0,
            payload: payload_value,
            messageType: message_type as i32,
            message: message_value,
        }
    }
}

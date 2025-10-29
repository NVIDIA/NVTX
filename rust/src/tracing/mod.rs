use crate::{domain::EventAttributes, Color, Domain, Payload};
use std::{collections::HashMap, marker::PhantomData, sync::Mutex};
use tracing_core::{
    field::{Field, Visit},
    span::{Attributes, Id, Record},
    Event, Subscriber,
};
use tracing_subscriber::{layer::Context, registry::LookupSpan, Layer};

/// The tracing layer for nvtx range and events.
///
/// **Supported fields**
/// * `category` (`&str`) provides a category within the target.
/// * `color`
///   * `&str` -- the valid names align the names provided by the color names
///     defined within [`crate::color`].
///   * `i32` -- a valid hex RGB value (masked with `0xFFFFFF`)
///   * `u32` -- a valid hex ARGB value (masked with `0xFFFFFFFF`)
/// * `payload` (one of: `{i,u,f}{32,64}` or `bool`) -- an additional value to track.
///
/// **Supported built-ins**
/// * the **target** indicates a domain name
/// * the **name** specifies the text for the Mark or Range.
///
/// ### `instrument` example:
///
/// ```
/// use tracing::instrument;
/// #[instrument(target = "domain", fields(color = 0xDDAA33, category = "cool", payload = k))]
/// fn baz (k : u64) {
///     std::thread::sleep(std::time::Duration::from_millis(10 * k));
/// }
/// ```
///
/// ### `mark` example:
///
/// ```
/// use tracing::info;
/// info!(name: "At the beginning of the program", target: "test", color = "blue");
/// ```
///
/// ### `span` example:
///
/// ```
/// use tracing::info_span;
/// let span = info_span!(target: "domain", "Running an arbitrary block!", color = "red", payload = 3.1415);
/// span.in_scope(|| {
///    // do work inside the span...
/// });
/// ```
pub struct NvtxLayer {
    /// The held domains of the layer.
    domains: Mutex<HashMap<String, crate::Domain>>,
}

impl Default for NvtxLayer {
    /// Create a new layer.
    fn default() -> NvtxLayer {
        NvtxLayer {
            domains: Mutex::new(HashMap::new()),
        }
    }
}

/// Data modeling [`EventAttributes`] without the need for lifetime management.
#[derive(Debug, Clone, Default)]
struct NvtxData {
    domain: String,
    category: Option<String>,
    color: Option<Color>,
    message: String,
    payload: Option<Payload>,
}

impl NvtxData {
    /// Make an event attributes struct from the current data, leaving it in-place
    fn event_attributes<'a>(&'a self, domain: &'a Domain) -> EventAttributes<'a> {
        let mut builder = domain.event_attributes_builder();
        if let Some(c) = &self.category {
            builder = builder.category_name(c.clone());
        }
        builder = builder.message(self.message.clone());
        if let Some(c) = &self.color {
            builder = builder.color(*c);
        }
        if let Some(p) = &self.payload {
            builder = builder.payload(*p);
        }
        builder.build()
    }
}

/// Wrapper around NVTX Range ids for span extension storage.
struct NvtxId(u64);

impl<S> Layer<S> for NvtxLayer
where
    S: Subscriber + for<'lookup> LookupSpan<'lookup>,
{
    fn on_event(&self, event: &Event<'_>, _ctx: Context<'_, S>) {
        let domain_name = event.metadata().target();
        let mut lock = self.domains.lock().unwrap();
        let domain = lock
            .entry(domain_name.to_string())
            .or_insert_with(|| Domain::new(domain_name));

        let mut data = NvtxData::default();
        let mut visitor = NvtxVisitor::<'_, S>::new(&mut data);
        event.record(&mut visitor);
        data.message = event.metadata().name().into();
        domain.mark(data.event_attributes(domain));
    }

    fn on_new_span<'a>(&'a self, attrs: &Attributes<'a>, id: &Id, ctx: Context<'a, S>) {
        let span = ctx.span(id).unwrap();
        let mut data = NvtxData::default();
        let mut visitor = NvtxVisitor::<'_, S>::new(&mut data);
        attrs.record(&mut visitor);
        data.domain = attrs.metadata().target().into();
        data.message = attrs.metadata().name().to_string();
        span.extensions_mut().insert(data);
    }

    fn on_record(&self, id: &Id, values: &Record<'_>, ctx: Context<'_, S>) {
        match ctx.span(id).unwrap().extensions_mut().get_mut::<NvtxData>() {
            Some(data) => {
                let mut visitor = NvtxVisitor::<'_, S>::new(data);
                values.record(&mut visitor);
            }
            None => todo!(),
        }
    }

    fn on_enter(&self, id: &Id, ctx: Context<'_, S>) {
        let mut range_id: Option<u64> = None;
        if let Some(data) = ctx.span(id).unwrap().extensions().get::<NvtxData>() {
            let domain_name = data.domain.clone();
            let mut lock = self.domains.lock().unwrap();
            let domain = lock
                .entry(domain_name.clone())
                .or_insert_with(|| Domain::new(domain_name));

            range_id = Some(domain.range_start(data.event_attributes(domain)));
        };
        if let Some(range) = range_id {
            ctx.span(id).unwrap().extensions_mut().insert(NvtxId(range));
        }
    }

    fn on_exit(&self, id: &Id, ctx: Context<'_, S>) {
        let span = ctx.span(id).unwrap();
        let data = span.extensions_mut().remove::<NvtxData>().unwrap();
        let domain_name = data.domain;
        let mut lock = self.domains.lock().unwrap();
        let domain = lock
            .entry(domain_name.clone())
            .or_insert_with(|| Domain::new(domain_name));

        let maybe_id = span.extensions_mut().remove::<NvtxId>();
        if let Some(NvtxId(id)) = maybe_id {
            domain.range_end(id)
        }
    }
}

struct NvtxVisitor<'a, S>
where
    S: Subscriber + for<'lookup> LookupSpan<'lookup>,
{
    data: &'a mut NvtxData,
    _traits: PhantomData<S>,
}

impl<'a, S> NvtxVisitor<'a, S>
where
    S: Subscriber + for<'lookup> LookupSpan<'lookup>,
{
    /// Create a new NvtxVisitor given a mutable data reference.
    fn new(data: &'a mut NvtxData) -> NvtxVisitor<'a, S> {
        NvtxVisitor {
            data,
            _traits: PhantomData,
        }
    }
}

impl<S> Visit for NvtxVisitor<'_, S>
where
    S: Subscriber + for<'lookup> LookupSpan<'lookup>,
{
    fn record_debug(&mut self, _field: &Field, _value: &dyn std::fmt::Debug) {}

    fn record_f64(&mut self, field: &Field, value: f64) {
        if field.name() == "payload" {
            self.data.payload = Some(Payload::Double(value));
        }
    }
    fn record_i64(&mut self, field: &Field, value: i64) {
        if field.name() == "payload" {
            self.data.payload = Some(Payload::Int64(value));
        } else if field.name() == "color" {
            let masked_value = value & 0xFFFFFFFF;
            if value == masked_value {
                self.data.color = Some((value as u32).into())
            }
        }
    }
    fn record_u64(&mut self, field: &Field, value: u64) {
        if field.name() == "payload" {
            self.data.payload = Some(Payload::Uint64(value));
        } else if field.name() == "color" {
            let masked_value = value & 0xFFFFFFFF;
            if value == masked_value {
                self.data.color = Some((value as u32).into())
            }
        }
    }
    fn record_bool(&mut self, field: &Field, value: bool) {
        if field.name() == "payload" {
            self.data.payload = Some(Payload::Int32(value as i32));
        }
    }
    fn record_str(&mut self, field: &Field, value: &str) {
        let owned = value.to_string();
        match field.name() {
            "color" => {
                if let Ok([r, g, b]) = color_name::css::Color::val().by_string(owned) {
                    self.data.color = Some(Color::new(r, g, b, 255));
                }
            }
            "category" => {
                self.data.category = Some(owned);
            }
            _ => (),
        }
    }
}

use crate::Str;

/// Enum for all CUDA types.
pub enum CudaResource {
    /// CuDevice
    Device(nvtx_sys::CuDevice),
    /// CuContext
    Context(nvtx_sys::CuContext),
    /// CuEvent
    Event(nvtx_sys::CuEvent),
    /// CuStream
    Stream(nvtx_sys::CuStream),
}

impl From<nvtx_sys::CuDevice> for CudaResource {
    fn from(value: nvtx_sys::CuDevice) -> Self {
        CudaResource::Device(value)
    }
}

impl From<nvtx_sys::CuContext> for CudaResource {
    fn from(value: nvtx_sys::CuContext) -> Self {
        CudaResource::Context(value)
    }
}

impl From<nvtx_sys::CuEvent> for CudaResource {
    fn from(value: nvtx_sys::CuEvent) -> Self {
        CudaResource::Event(value)
    }
}

impl From<nvtx_sys::CuStream> for CudaResource {
    fn from(value: nvtx_sys::CuStream) -> Self {
        CudaResource::Stream(value)
    }
}

/// Name a CUDA Resource (one of: Device, Context, Event, or Stream).
///
/// ```
/// nvtx::name_cuda_resource(nvtx::CudaResource::Device(0), "GPU 0");
/// /// or implicitly:
/// nvtx::name_cuda_resource(0, "GPU 0");
/// ```
pub fn name_cuda_resource(resource: impl Into<CudaResource>, name: impl Into<Str>) {
    match resource.into() {
        CudaResource::Context(context) => match name.into() {
            Str::Ascii(s) => unsafe { nvtx_sys::name_cucontext_ascii(context, &s) },
            Str::Unicode(s) => unsafe { nvtx_sys::name_cucontext_unicode(context, &s) },
        },
        CudaResource::Device(device) => match name.into() {
            Str::Ascii(s) => nvtx_sys::name_cudevice_ascii(device, &s),
            Str::Unicode(s) => nvtx_sys::name_cudevice_unicode(device, &s),
        },
        CudaResource::Event(event) => match name.into() {
            Str::Ascii(s) => unsafe { nvtx_sys::name_cuevent_ascii(event, &s) },
            Str::Unicode(s) => unsafe { nvtx_sys::name_cuevent_unicode(event, &s) },
        },
        CudaResource::Stream(stream) => match name.into() {
            Str::Ascii(s) => unsafe { nvtx_sys::name_custream_ascii(stream, &s) },
            Str::Unicode(s) => unsafe { nvtx_sys::name_custream_unicode(stream, &s) },
        },
    }
}

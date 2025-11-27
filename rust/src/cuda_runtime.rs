use crate::Str;

/// Enum for all CUDA Runtime types.
pub enum CudaRuntimeResource {
    /// Device (integer ID)
    Device(i32),
    /// Event
    Event(nvtx_sys::CudaEvent),
    /// Stream
    Stream(nvtx_sys::CudaStream),
}

impl From<i32> for CudaRuntimeResource {
    fn from(value: i32) -> Self {
        CudaRuntimeResource::Device(value)
    }
}

impl From<nvtx_sys::CudaEvent> for CudaRuntimeResource {
    fn from(value: nvtx_sys::CudaEvent) -> Self {
        CudaRuntimeResource::Event(value)
    }
}

impl From<nvtx_sys::CudaStream> for CudaRuntimeResource {
    fn from(value: nvtx_sys::CudaStream) -> Self {
        CudaRuntimeResource::Stream(value)
    }
}

/// Name a CUDA Runtime Resource (one of: Device, Event, or Stream).
///
/// ```
/// nvtx::name_cudart_resource(nvtx::CudaRuntimeResource::Device(0), "GPU 0");
/// /// or implicitly:
/// nvtx::name_cudart_resource(0, "GPU 0");
/// ```
pub fn name_cudart_resource(resource: impl Into<CudaRuntimeResource>, name: impl Into<Str>) {
    match resource.into() {
        CudaRuntimeResource::Device(device) => match name.into() {
            Str::Ascii(s) => nvtx_sys::name_cuda_device_ascii(device, &s),
            Str::Unicode(s) => nvtx_sys::name_cuda_device_unicode(device, &s),
        },
        CudaRuntimeResource::Event(event) => match name.into() {
            Str::Ascii(s) => unsafe { nvtx_sys::name_cuda_event_ascii(event, &s) },
            Str::Unicode(s) => unsafe { nvtx_sys::name_cuda_event_unicode(event, &s) },
        },
        CudaRuntimeResource::Stream(stream) => match name.into() {
            Str::Ascii(s) => unsafe { nvtx_sys::name_cuda_stream_ascii(stream, &s) },
            Str::Unicode(s) => unsafe { nvtx_sys::name_cuda_stream_unicode(stream, &s) },
        },
    }
}

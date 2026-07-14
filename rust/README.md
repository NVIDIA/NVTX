# nvtx-rs

nvtx-rs provides Rust bindings for NVIDIA's nvtx library.

See the generated documentation for more details and the examples for sample usage.

## Feature profiles

The crate now supports three capability tiers:

- `std` (default): full ergonomic API, domain caching, thread helpers, and tracing integration
- `alloc`: `#![no_std]` + heap-backed ergonomic API (no `std`-only integrations). Domain
  caches use a spinlock in this profile, so interrupt-driven or RTOS environments should
  account for priority inversion risk around domain operations.
- core-only (`--no-default-features`): strict `#![no_std]` baseline with low-level borrowed-string APIs

### Cargo feature examples

```bash
# default full API
cargo add nvidia-nvtx

# no_std + alloc profile
cargo add nvidia-nvtx --no-default-features --features alloc

# strict core profile
cargo add nvidia-nvtx --no-default-features
```

In strict core mode, use borrowed C-string APIs such as `mark_ascii` / `mark_unicode`,
`name_thread_ascii` / `name_thread_unicode`, or the low-level exports under `nvtx::sys`.

## Extension APIs

The default profile enables the CUDA, CUDA Runtime, OpenCL, extended-payload,
counter, memory, and CUDA memory bindings. These can also be selected
individually with:

- `payload`: schemas, enums, structured payload events, scopes, time domains,
  deferred events, and counter/scope/time/correlation semantics
- `counters`: typed counters and batches; implies `payload`
- `memory`: heaps, regions, permission sets, and scoped permission bindings
- `memory_cuda_runtime`: CUDA memory permissions and initialization annotations;
  implies `memory` and `cuda_runtime`

Ergonomic extension builders require `alloc`. The corresponding generated FFI
and thin wrappers under `nvtx::sys` remain available in core-only builds when
their extension feature is enabled.

See the `extended_payload`, `counters`, and `memory` examples for end-to-end
usage.

## Compatibility

The `nvtx-rs` crate requires rustc 1.77 or greater

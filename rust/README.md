# Rust NVTX

Rust NVTX provides Rust language bindings for the NVIDIA Tools Extension SDK (NVTX).

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
cargo add nvtx

# no_std + alloc profile
cargo add nvtx --no-default-features --features alloc

# strict core profile
cargo add nvtx --no-default-features
```

In strict core mode, use borrowed C-string APIs such as `mark_ascii` / `mark_unicode`,
`name_thread_ascii` / `name_thread_unicode`, or the low-level exports under `nvtx::sys`.

## Compatibility

The `nvtx` crate requires rustc 1.77 or greater.

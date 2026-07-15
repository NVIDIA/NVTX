<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

# NVTXW User Guide {#NVTXW_USER_GUIDE}

\tableofcontents

NVTXW ("NVTX Writer") is an offline C API for delivering already-collected trace data to tools as
NVTX events, counters, and timing information through an NVTXW backend.

It reuses the NVTX payload, counter, scope, and time-domain schema types, but lets a *producer* pass
pre-collected data to a *backend* (typically supplied by a tool such as Nsight Systems) that writes
it out, merges it into an existing report, or visualizes it.

NVTXW is designed for programmatic offline import. Instead of forcing trace data through an
intermediate interchange file, you call a C API that can carry binary payloads, counters, explicit
time domains, scopes, and multiple streams directly to a tool-specific backend.

This guide explains the model, begins with a helper-based quick start, and then covers the core
interface, configuration, versioning, and error handling.

**Headers covered here**
- `nvtxw3/nvtxw3.h` — the core producer/backend contract.
- `nvtxw3/nvtxw3_loader.h` — optional reference loader and config utilities.
- `nvtxw3/nvtxw3_helpers.h` — convenience helpers (setup, events, counters).

---

## 1. When to Use NVTXW

Regular NVTX instrumentation works by intercepting NVTX API calls *as they happen* in a running,
instrumented process. NVTXW addresses a different situation: you already *have* trace data and want
to deliver it to a tool. For example:

- A custom profiler, runtime, or framework that collected its own timing data and wants to render it
  on a tool's timeline.
- Replaying a previously captured trace into a tool.
- Emitting counters/markers/ranges from data that was produced off the critical path, outside live
  injection.

Common workflows include:

- A converter that reads a third-party trace file (e.g.
  [OTF2](https://www.vi-hps.org/projects/score-p/) or an application-specific log) and creates a new
  tool report through the selected backend.
- A converter that merges foreign trace data into an existing profiler report.
- An existing post-processing tool that exports NVTX events directly via NVTXW while it processes
  its own native logs, avoiding an intermediate conversion file.
- An analysis script that augments one or more reports with findings, derived ranges, or counters.
- A synthetic-report generator for demos, UI testing, or reproducing rare event sequences.

In all these cases the data is "feed-forward": you produce it, hand it to the backend, and the
backend consumes it. You supply timestamps explicitly rather than relying on the tool to capture
them at call time.

### NVTXW vs. NVTX and Deferred Events

- **NVTX** is online instrumentation: the instrumented process calls NVTX while the tool is tracing
  it. NVTXW is offline import/export: a producer feeds data it already collected into a backend
  after, or outside, the original run.
- **NVTX deferred events** are still online NVTX calls from the program being traced. NVTXW is an
  offline way to write deferred events, ranges, counters, payloads, and timing data into a tool.

---

## 2. The Producer/Backend Model

NVTXW is the producer/backend interface defined by `nvtxw3/nvtxw3.h`. In a typical deployment,
these terms are used around that interface:

| Term | What it means |
| --- | --- |
| **Producer** | Code that owns or reads already-collected trace data and calls the NVTXW interface. This might be an application, converter, profiler, analysis script, or post-processing tool. |
| **Backend** | An implementation of the NVTXW interface. It consumes producer calls and decides what to do with the data: write a file, merge a report, open a viewer, and so on. |
| **Reference loader** (optional) | Optional helper code provided alongside the NVTXW headers that locates a backend library, resolves `nvtxwGetInterface`, and returns the versioned function table. It lives in `nvtxw3/nvtxw3_loader.h` and `src/nvtxw3_loader.c`; producers may use it, adapt it, or replace it. |

Backends define the destination and behavior for the data. The reference loader can locate a backend
from `NVTXW3_LIBRARY`, from an explicit filename/path, or from its default search locations. A
producer may expose any policy it wants around that choice, such as a command-line option, UI
setting, configuration file, or fully custom loading code. Supporting NVTX events in a tool does not
automatically mean that tool supports NVTXW; the tool must provide or use an NVTXW backend.

The producer obtains the backend's **function table** and calls through it. The headers expose it as
`nvtxwInterface_t`, an alias for the current versioned table `nvtxwInterface_v2_t`; the rest of this
guide uses `nvtxwInterface_t` except where the version itself matters. The objects exposed by that
interface form a hierarchy:

```
Session                          (a collection of trace data)
├── Domain                       (namespace for schemas, scopes, counters, …)
│   ├── Registered strings
│   ├── Scopes
│   ├── Payload / enum schemas
│   ├── Counters
│   └── Time domains
└── Stream                       (destination for events and counters)
    ├── Events  (marks, ranges)
    └── Counter samples
```

- A **session** represents one collection of trace data and owns its domains, streams, scopes, and
  handles. In report-producing backends, one session often maps to one output report, or to one
  start/stop capture interval that is being augmented. Ending the session is what tells the backend
  "all data is in" and may trigger writing a file, merging a report, or opening a viewer.
- A **domain** is a namespace, just as in NVTX. Use it to identify the library, framework, or
  producer of the data, not the physical source of the event. Everything referenced by data in a
  stream (schemas, scopes, counters, registered strings, time domains) must be registered in that
  stream's domain. Domains let independent producers avoid ID collisions.
- A **stream** is the object that receives events and counters. It usually represents a logical
  worker or partition such as a process, thread, MPI rank, runtime worker, or GPU stream. A stream
  carries an implicit domain, optional defaults such as scope and time domain, and ordering options
  that tell the backend what assumptions it can make. Events and counters can override the stream
  defaults where the API supports it.
- A **scope** describes where the data belongs in a logical or physical hierarchy: process, thread,
  GPU, NIC, storage device, OS source, cluster resource, or an application-defined concept. Scopes
  anchor data in the tool hierarchy; domains keep producers separate.

Lifetime rules:

- Session handles are valid until `SessionEnd`.
- Domain and scope handles are owned by the session and valid until `SessionEnd`.
- Stream handles are valid until `StreamClose`, or until `SessionEnd` of the owning session.
- Register an entity (schema, scope, counter, …) **before** anything that references it, so the
  backend can resolve IDs in a single pass.

---

## 3. Build Setup

The NVTXW front-end API is plain C, compatible with C90-style consumers, and free of external
runtime dependencies. The core API is header-only and is included with the NVTX C distribution (the
`nvtx3-c` include path covers both `nvtx3/` and `nvtxw3/`). The only compiled piece in the NVTXW
front end is the **optional reference loader**, exposed as the CMake target `nvtx3::nvtxw3-loader`.

If you choose to use the reference loader and you build with CMake, you can link its target:

```cmake
add_executable(my_writer main.c)
target_link_libraries(my_writer PRIVATE nvtx3::nvtxw3-loader)
```

If you choose to load the backend yourself (your own `dlopen`/`LoadLibrary`), you only need the
headers — link `nvtx3::nvtx3-c` and skip the loader.

If you do not use CMake, put `include/` on the header search path. If you also use the reference
loader, compile `src/nvtxw3_loader.c` into your build and link the dynamic-loader library (`-ldl`) where
required.

NVTXW is normally used where the backend tool runs, such as a host-side report conversion
environment, rather than on a compute node during the original profiled run. That is a practical
distinction from online NVTX: conversion and report writing can happen after the application
execution has finished.

---

## 4. Quick Start (With Helpers)

The `nvtxw3_helpers.h` umbrella header pulls in stateless helpers that reduce the common producer
flow to a few calls. These helpers are convenience code, not the long-term compatibility boundary
of NVTXW. Treat them as a starting point or template for your own custom helpers when you need a
different policy or abstraction.

The fastest path is:

1. Load a backend and get the interface table.
2. Begin a session.
3. Register a domain.
4. Open a stream **and** build an event writer in one call.
5. Write events.
6. Close the stream and end the session.

```c
#include <nvtx3/nvToolsExt.h>
#include <nvtxw3/nvtxw3_helpers.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    nvtxwResultCode_t rc = NVTXW_RESULT_SUCCESS;

    /* 1. Load the backend using the default search (overridable via NVTXW3_LIBRARY). */
    const nvtxwInterface_t* iface = NULL;
    if ((rc = nvtxwLoadInterfaceDefault(&iface, NULL)) != NVTXW_RESULT_SUCCESS)
        return rc;

    /* 2. Begin a session. */
    nvtxwSessionAttributes_t sessionAttr;
    nvtxwSessionAttributesInit(&sessionAttr);
    sessionAttr.name = "my-session";
    sessionAttr.configString = NULL; /* tool-specific options, key=value lines */

    nvtxwSessionHandle_t session = NULL;
    if ((rc = iface->SessionBegin(&sessionAttr, &session)) != NVTXW_RESULT_SUCCESS)
        return rc;

    /* 3. Register a domain (NULL/"" => the session default domain). */
    nvtxDomainHandle_t domain = NULL;
    if ((rc = nvtxwDomainRegister(iface, session, "my-domain", &domain)) != NVTXW_RESULT_SUCCESS)
        return rc;

    /* 4. Open a stream and get a ready-to-use event writer. */
    nvtxwEventWriter_t writer;
    if ((rc = nvtxwEventWriterOpen(iface, session, "my-stream", domain,
            /* scopePath */ NULL, /* timeDomainId */ NVTX_TIME_DOMAIN_ID_NONE,
            &writer)) != NVTXW_RESULT_SUCCESS)
        return rc;

    /* 5. Write a marker and a complete range with explicit timestamps. */
    nvtxwEventAttributesUtf8_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.color   = 0xFF00FF00u; /* ARGB; 0 means none */
    attr.message = "hello from NVTXW";
    nvtxwMarkWriteUtf8(&writer, /* timestamp */ 100, attr);

    attr.message = "a range";
    nvtxwRangePushPopWriteUtf8(&writer, /* begin */ 200, /* end */ 350, attr);

    /* 6. End the session (flushes/writes output). */
    iface->StreamClose(writer.stream);
    return iface->SessionEnd(session);
}
```

Notes about this flow:

- `nvtxwEventWriterOpen` is the one-call helper for the common "one stream per domain" case. It
  registers the full set of event schemas, opens the stream, and fills in an `nvtxwEventWriter_t`
  bundling the interface, stream, and schema IDs. You then pass that writer to the event write
  helpers.
- The `*Utf8` variants take a UTF-8 string message directly; the non-UTF-8 variants take a
  registered string handle (see [Domains, Scopes, and Registered Strings](#NVTXW_DOMAINS_SCOPES_STRINGS)).
- Timestamps are plain `int64_t` values that you provide. Their meaning is defined by the stream's
  time domain (see [Time Domains, Timestamps, and Synchronization](#NVTXW_TIME_DOMAINS)).
- Passing `NULL` for the loader's module output keeps the backend loaded until process exit. If you
  want explicit clean unload, keep the module handle and call `nvtxwUnload` when done.

If you want several streams to share one domain, you can either use the core API directly
(`StreamOpen`, `EventWrite`, and related calls), or stay with the helpers and avoid re-registering
schemas per stream: register the schemas once with `nvtxwEventSchemasRegister`, build one writer
with `nvtxwEventWriterInit`, then copy that writer and swap in each stream.

---

## 5. Writing Events {#NVTXW_WRITING_EVENTS}

For common NVTX-style marks and ranges, the helper API provides compact wrappers around the core
`EventWrite` call. All event helpers take an `nvtxwEventWriter_t*` plus timestamp(s) and event
attributes. Two attribute structs are available:

- `nvtxwEventAttributesUtf8_t` — message is a UTF-8 string. A `messageLength` of 0 means
  "null-terminated, use `strlen`"; a non-zero value is a byte count used verbatim (so the string
  need not be null-terminated). UTF-8 is used as a stable byte-level contract, independent of the
  producer's locale, and includes ASCII as a subset. `color` and `category` are optional (0 = none).
  To attach a human-readable name to a category ID, register it with `CategoryRegister` (see
  [Domains, Scopes, and Registered Strings](#NVTXW_DOMAINS_SCOPES_STRINGS)).
- `nvtxwEventAttributes_t` — message is a **registered** string handle obtained from
  `StringRegister` (see [Domains, Scopes, and Registered Strings](#NVTXW_DOMAINS_SCOPES_STRINGS)).
  Use this when the same message recurs many times.

| Helper | Meaning |
| --- | --- |
| `nvtxwMarkWrite[Utf8]` | A marker at a single timestamp. |
| `nvtxwRangePushPopWrite[Utf8]` | A complete push/pop range (begin + end). |
| `nvtxwRangeStartEndWrite[Utf8]` | A complete start/end range (begin + end). |
| `nvtxwRangePushWrite[Utf8]` | The begin half of a push/pop range. |
| `nvtxwRangePopWrite` | The end half of a push/pop range. |
| `nvtxwRangeStartWrite[Utf8]` | The begin half of a start/end range; takes an `nvtxRangeId_t`. |
| `nvtxwRangeEndWrite` | The end half of a start/end range; takes the matching `nvtxRangeId_t`. |

Here, `[Utf8]` means both forms exist: the base helper takes `nvtxwEventAttributes_t`, and the
`Utf8` form takes `nvtxwEventAttributesUtf8_t`.

Use the *PushPop* / *StartEnd* variants when you already know both endpoints. Use the split
*Push*/*Pop* or *Start*/*End* variants when you write the two ends at different times. As in NVTX,
push/pop ranges nest like a stack; start/end ranges are correlated by an explicit
`nvtxRangeId_t` and may overlap.

Example with a registered string and a start/end range:

```c
nvtxStringHandle_t msg = NULL;
iface->StringRegister(domain, "frequently-used label", &msg);

nvtxwEventAttributes_t a;
memset(&a, 0, sizeof(a));
a.message = msg;          /* registered handle, not a char* */
a.color   = 0xFFFF8800u;

nvtxRangeId_t id = 1234;  /* your correlation id */
nvtxwRangeStartWrite(&writer, /* timestamp */ 500, id, a);
/* ... later ... */
nvtxwRangeEndWrite(&writer, /* timestamp */ 900, id);
```

---

## 6. Domains, Scopes, and Registered Strings {#NVTXW_DOMAINS_SCOPES_STRINGS}

**Domains** are namespaces. In the core API, fill an `nvtxwDomainAttributes_t` and call
`DomainRegister` from the function table. Passing `NULL` or an empty name selects the session
default domain. Re-registering the same name in a session returns the same handle, so different
parts of your producer can share registrations. The `nvtxwDomainRegister` helper wraps this for the
common name-only case. A domain is owned by its session, not by a stream: multiple streams may share
one domain, and all of them see the schemas, enums, scopes, counters, registered strings, and
resources registered in it.

**Registered strings** (`StringRegister`) let you register a message string once and refer to it by
handle in many events. Prefer this for repeated messages: it keeps output smaller and lets the
backend do per-string work (logging, matching) once.

**Scopes** describe where events and counters belong in a logical or physical hierarchy. In the core
API, fill an `nvtxScopeAttr_t` and call `ScopeRegister` from the function table. The
`nvtxwScopeRegister` helper covers the common path-string case (e.g. `"gpu/stream0"`) under
the root scope. A stream has a default scope; per-event scope can override it where supported. Use
scopes for the origin or placement of data: thread, process, GPU, device, node, VM/container,
network switch, or a framework-specific worker. Use domains for the producer namespace.
`NVTX_SCOPE_NONE` and `NVTX_SCOPE_ROOT` can be used directly; other concrete scopes must be
registered in the stream's domain. Runtime-resolved scopes (`NVTX_SCOPE_CURRENT_*`) are invalid as
registered scope IDs and as stream or counter defaults. The two system anchors
`NVTX_SCOPE_CURRENT_HW_MACHINE` and `NVTX_SCOPE_CURRENT_VM` are an exception: the core
`ScopeRegister` API accepts them as `nvtxScopeAttr_t::parentScope` values, allowing a backend to
resolve a path against the output report's machine or VM when unambiguous. The
`nvtxwScopeRegister` helper always uses `NVTX_SCOPE_ROOT`; use the core API to select a system
anchor.

Some tools append the NVTX domain beneath the selected scope when displaying the hierarchy, so two
producers can report data for the same physical or logical source without colliding. Tool-specific
documentation should describe the exact rendering and any merge behavior.

**Resources** can be registered with `ResourceRegister` (mirroring NVTX's
`nvtxDomainResourceCreate`) to attach stable names or metadata to objects such as hosts, processes,
threads, devices, or other entities.

**Categories** group events for filtering and sorting. Events carry a category as an integer ID in
their `category` attribute (the value behind `NVTX_PAYLOAD_ENTRY_TYPE_CATEGORY`); a value of 0 means
"no category". To give that ID a human-readable name, call `CategoryRegister` (mirroring NVTX's
`nvtxDomainNameCategory`), or use the `nvtxwCategoryRegister` helper. Names are tracked per domain,
so the same ID can be named independently in different domains.

---

## 7. Time Domains, Timestamps, and Synchronization {#NVTXW_TIME_DOMAINS}

You supply timestamps as plain `int64_t` values. A **time domain** describes the clock and unit
behind those values. In the core API, fill an `nvtxTimeDomainAttr_t` and call `TimeDomainRegister`
from the function table. Set a stream default time domain with
`nvtxwStreamAttributes_t::timeDomainId` when calling `StreamOpen`; a per-event or per-counter time
domain overrides it. The `nvtxwTimeDomainRegister`, `nvtxwStreamOpen`, and `nvtxwEventWriterOpen`
helpers cover the common simple cases.

A time domain is **optional**. With none (`NVTX_TIME_DOMAIN_ID_NONE`), a tool places events on a
relative timeline from the raw values. That is fine when the data stands on its own. Register one
when the tool must relate your timestamps to another clock, e.g. to merge into an existing
report:

- A predefined `NVTX_TIMESTAMP_TYPE_*` value names a known clock.
- `NVTX_TIMESTAMP_TYPE_NONE` names no clock, but setting `timerResolution` (ticks per second) and
  `timerStart` still lets a tool interpret durations and build an absolute timeline for a custom
  clock. These extra attributes require the core `TimeDomainRegister`; the helper sets only the
  type.

When a backend needs to compare or combine timestamps from different time domains, provide the
relationship between them. This can matter when merging into an existing report, and also when two
streams in the same import use different clocks:

- `TimeSyncPointWrite` — records that `timestamp1` (in `timeDomainId1`) and `timestamp2` (in
  `timeDomainId2`) denote the same instant. Two such points let a tool derive the conversion.
- `TimeSyncPointTableWrite` — a batch of such sync points between a source and destination domain.
- `TimestampConversionFactorWrite` — a `slope` plus an anchor pair, giving the linear conversion
  directly (an alternative to two sync points).

### Stream Ordering

`nvtxwStreamAttributes_t` lets you tell the backend how sorted your data is, so it can avoid
re-sorting. These are set when opening a stream via `StreamOpen` (the helper
`nvtxwStreamOpen` applies safe defaults):

- `orderInterleaving` — tells the backend whether the ordering options apply stream-wide or
  per-scope. Use
  `NVTXW_STREAM_ORDER_INTERLEAVING_NONE` when they apply to the stream as a whole. Use
  `NVTXW_STREAM_ORDER_INTERLEAVING_SCOPE` when events or counters from multiple scopes may be
  interleaved in the stream, and the ordering options apply only within each same-scope sequence.
- `orderingType` — tells the backend what "sorted" means for this stream. Use
  `NVTXW_STREAM_ORDERING_TYPE_UNKNOWN` when you cannot make an ordering claim. Use
  `NVTXW_STREAM_ORDERING_TYPE_STRICT` when entries are written in timestamp order. If the stream
  contains ranges and you group them by one endpoint, use
  `NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_START` when ranges are ordered by begin time, or
  `NVTXW_STREAM_ORDERING_TYPE_PACKED_RANGE_END` when they are ordered by end time.
- `orderingSkid` + `orderingSkidAmount` — describe how far out of order entries may be when the
  stream is only partially sorted. Use `NVTXW_STREAM_ORDERING_SKID_NONE` when the ordering claim is
  exact. Use `NVTXW_STREAM_ORDERING_SKID_TIME_NS` when later entries may move backward by at most a
  fixed number of nanoseconds. Use `NVTXW_STREAM_ORDERING_SKID_EVENT_COUNT` when at most a fixed
  number of following entries may have an earlier timestamp.

If you cannot make ordering guarantees, leave the defaults (`UNKNOWN` / `SKID_NONE`) and the backend
will sort as needed.

---

## 8. Counters

Counters are time series of numeric samples. In the core API, register a counter in a domain with
`CounterRegister`, then write samples to a stream with `CounterWrite`, `CounterNoValueWrite`, or
`CounterBatchWrite`. The scalar helpers shown below cover the common single-value cases.

```c
uint64_t intCounterId = 0;
nvtxwCounterInt64Register(iface, domain, "queue-depth", /* semantics */ NULL,
                          &intCounterId);
nvtxwCounterInt64Write(iface, stream, /* timestamp */ 100, intCounterId, 42);

uint64_t fpCounterId = 0;
nvtxwCounterFloat64Register(iface, domain, "utilization", NULL, &fpCounterId);
nvtxwCounterFloat64Write(iface, stream, /* timestamp */ 100, fpCounterId, 0.75);
```

- `CounterRegister` is the general form: pass a scalar entry type (e.g.
  `NVTX_PAYLOAD_ENTRY_TYPE_INT64`, `NVTX_PAYLOAD_ENTRY_TYPE_DOUBLE`) or a full schema ID for a
  multi-value counter group, and optional semantics.
- For multi-value or custom-layout counters, register the schema, register the counter with that
  schema ID, and write samples through the core `CounterWrite` / `CounterBatchWrite` with the
  matching binary layout.
- `CounterNoValueWrite` records a sample that has *no* value, with an `NVTX_COUNTER_SAMPLE_*` reason
  explaining why (e.g. a gap).
- Counter time semantics override the stream default time domain.

---

## 9. The Core Interface

The core contract is the function table, `nvtxwInterface_t`. When you need full
control, such as custom schemas, batches, or multiple payloads per event, call the relevant function
table entry directly. You can obtain the table by resolving the backend symbol yourself, or let a
loader helper do that setup for you. The convenience helpers are thin, stateless wrappers over this
table.

| Member | Purpose |
| --- | --- |
| `SessionBegin` / `SessionEnd` | Create / finalize a session. |
| `DomainRegister` | Register or retrieve a domain. |
| `CategoryRegister` | Name an NVTX category ID. |
| `StringRegister` | Register an immutable string and return a reusable handle. |
| `ResourceRegister` | Register resource metadata. |
| `ScopeRegister` | Register a scope, get a scope ID. |
| `SchemaRegister` / `EnumRegister` | Register a payload schema / enum schema. |
| `CounterRegister` | Register a counter or counter group. |
| `TimeDomainRegister` | Register a time domain. |
| `StreamOpen` / `StreamClose` | Open / close a stream. |
| `EventWrite` / `EventBatchWrite` | Write one event / a batch sharing a schema. |
| `CounterWrite` / `CounterNoValueWrite` / `CounterBatchWrite` | Write counter samples. |
| `TimeSyncPointWrite` / `TimeSyncPointTableWrite` / `TimestampConversionFactorWrite` | Relate time domains. |

`EventWrite` takes an array of `nvtxPayloadData_t`: the payloads together form a single logical
event. Each payload's `schemaId` defines its byte layout, and entries may reference other payloads
by index. The UTF-8 event helpers use a fixed header payload plus a separate standalone string
payload for the message, so they do not require backend support for deep-copying inline strings.

**Static vs. dynamic IDs.** Each `*Register` call returns a dynamic ID via the `*Out` pointer.
Alternatively, you can supply your own *static* ID in the attr struct, which must be unique within
the domain and fall in the static range (`>= *_STATIC_START` and `< *_DYNAMIC_START`) for that ID
kind. Schema and enum IDs share one namespace within a domain.

---

## 10. Loading the Backend

You have two options.

### Option A — The Reference Loader (Recommended Default)

The reference loader in `nvtxw3/nvtxw3_loader.h` locates and loads a backend library. There are two
layers:

- **Loader API.** `nvtxwLoad(library, &getInterface, &module)` finds the backend, loads it,
  and resolves its exported `nvtxwGetInterface` symbol. You then call
  `getInterface(NVTXW_INTERFACE_VERSION, …)` yourself to obtain the table, `nvtxwUnload(module)` to
  unload, and `nvtxwGetError(rc, …)` to format a result code for diagnostics. Use this layer when
  you want the loader to handle library discovery but still request the interface version yourself
  (the [Core Interface Example](#NVTXW_CORE_EXAMPLE) does this).
- **Convenience helper.** `nvtxwLoadInterface` / `nvtxwLoadInterfaceDefault`, in
  `nvtxw3/nvtxw3_setup_helpers.h`, wrap `nvtxwLoad` and the `getInterface` call into a single
  step that hands back a ready-to-use interface table:

```c
const nvtxwInterface_t* iface = NULL;
nvtxwModuleHandle_t module = NULL;

/* Convenience helper for the default reference-loader search. */
nvtxwResultCode_t rc = nvtxwLoadInterfaceDefault(&iface, &module);
```

`nvtxwLoad` (and therefore `nvtxwLoadInterface`) resolves the backend in priority order:

1. The **`NVTXW3_LIBRARY`** environment variable, if set to a non-empty value (passed verbatim to
   `dlopen`/`LoadLibrary`). This lets a tool or launcher redirect the backend without rebuilding
   your application.
2. The **`library`** argument, if non-NULL (a filename or path).
3. If `library` is NULL, a **default search**: the executable directory, the standard
   dynamic-library search paths, then the current working directory, looking for `libnvtxw3.so` /
   `nvtxw3.dll` (platform-dependent name).

When the environment variable or the `library` argument names a specific library, that library is
the *only* candidate — a load failure or missing symbol is reported rather than silently falling
back.

If your application accepts a backend path from a user, command line, or config file, prefer
requiring an absolute path and validate it before loading. The reference loader ultimately calls the
platform dynamic loader (`dlopen` or `LoadLibrary`), so the usual dynamic-library path risks apply.

When using the reference loader, keep the returned `module` handle alive for as long as you use the
interface. Call `nvtxwUnload(module)` when done (optional; mainly for clean teardown and
memory-checker hygiene). If you pass `NULL` for `moduleOut`, the backend stays loaded until process
exit.

### Option B — Custom Loading

Load the backend library by your chosen mechanism, resolve its single exported symbol
`nvtxwGetInterface` (`NVTXW_GET_INTERFACE_SYMBOL_NAME`), and call it with the version you want:

```c
nvtxwGetInterface_t getInterface = /* dlsym(handle, "nvtxwGetInterface") */;
const void* ifacePtr = NULL;
if (getInterface(NVTXW_INTERFACE_VERSION, &ifacePtr) == NVTXW_RESULT_SUCCESS)
{
    const nvtxwInterface_t* iface = (const nvtxwInterface_t*)ifacePtr;
    /* ... use iface ... */
}
```

With custom loading, the producer code also owns unload behavior. A backend can optionally export
`nvtxwFinalize` to free process-global state before unload; the reference loader's
`nvtxwUnload` calls it if present.

---

## 11. Configuration

The NVTXW interface defines one configuration field:
`nvtxwSessionAttributes_t::configString`, passed to `SessionBegin`. Backends use this for
tool-specific session options, such as naming an output report or merging the NVTXW data into an
existing report.

Any load-time or backend-global configuration is outside the NVTXW interface contract. A producer
and backend may agree on their own mechanism, such as a backend-specific environment variable, but
NVTXW does not standardize one. (`NVTXW3_LIBRARY` is separate — it only tells the reference loader
*which* library to load.)

The session config string format is `key=value`, one entry per line, delimited by a line break
(CR/LF) or `|` (pipe). Keys must not contain `=`; values must not contain `\r`, `\n`, or `|`. Lines beginning with
`#` are comments. If a key appears more than once, the **first** occurrence wins — so you can
override by prepending or default by appending. Keys are tool-specific; backends ignore keys they
do not recognize and use reasonable defaults for missing ones. See your tool's documentation for
supported keys.

You can parse/modify a config string yourself with `nvtxwConsumeConfigString`, which invokes a
callback for each key/value pair.

---

## 12. Versioning and Forward Compatibility {#NVTXW_VERSIONING}

The producer requests a specific **interface version** from `nvtxwGetInterface`.
`NVTXW_INTERFACE_VERSION` is currently `2`, represented by `nvtxwInterface_v2_t`. If the backend
does not support the requested version, it returns `NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED`.

Within a major version, new functionality is added in trailing `reserved` slots without changing the
table size or any offset. Baseline members in the current `nvtxwInterface_v2_t` are always
implemented; only members added by later minor versions can be NULL on older backends. A backend
that does not support a baseline operation still implements the member and returns
`NVTXW_RESULT_NOT_SUPPORTED` from it, rather than leaving the slot NULL. Before calling a future
member documented as optional, check it with `NVTXW_INTERFACE_HAS(iface, member)`, which is non-zero
when `iface` is non-NULL and `member` is non-NULL.

---

## 13. Thread Safety {#NVTXW_THREAD_SAFETY}

Unless the backend documents otherwise, a producer can only assume the following minimum
thread-safety guarantee: distinct streams can be written concurrently from different threads without
producer synchronization. A producer should not assume that concurrent writes to a single stream are
safe. Likewise, lifecycle and registration calls (`SessionBegin`, `StreamOpen`, `StreamClose`,
`SessionEnd`, and the `*Register` functions) are not assumed thread-safe; serialize them and do not
overlap them with writes.

---

## 14. Error Handling

Interface calls and loader setup helpers that can fail return an `nvtxwResultCode_t`:

| Code | Meaning |
| --- | --- |
| `NVTXW_RESULT_SUCCESS` (0) | Success. |
| `NVTXW_RESULT_FAILED` (1) | Generic failure. |
| `NVTXW_RESULT_INVALID_ARGUMENT` (2) | A bad argument (e.g. NULL handle). |
| `NVTXW_RESULT_LIBRARY_NOT_FOUND` (3) | Backend library could not be located. |
| `NVTXW_RESULT_LIBRARY_LOAD_FAILED` (4) | Library found but failed to load. |
| `NVTXW_RESULT_LIBRARY_SYMBOL_MISSING` (5) | `nvtxwGetInterface` symbol not found. |
| `NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED` (6) | Backend has no table for the requested version. |
| `NVTXW_RESULT_NOT_SUPPORTED` (7) | Valid call, but the backend does not support the operation or feature. |

In production code, check the result of calls whose success you rely on, and stop or clean up on
failure. `NVTXW_RESULT_NOT_SUPPORTED` is not a hard failure: it reports that a specific operation or
feature is unavailable on this backend, so a producer can skip or degrade that feature and continue.
A missing function (a null slot, checked with `NVTXW_INTERFACE_HAS`; see
[Versioning and Forward Compatibility](#NVTXW_VERSIONING)) and an unsupported interface version
(`NVTXW_RESULT_INTERFACE_VERSION_NOT_SUPPORTED`) are separate cases. The convenience helpers
(`nvtxw3_helpers.h`) validate their arguments and return `NVTXW_RESULT_INVALID_ARGUMENT` before
calling into the backend. If you use the reference loader, `nvtxwGetError` can format these result
codes for diagnostics:

```c
char buf[128];
nvtxwGetError(rc, buf, sizeof(buf));
fprintf(stderr, "NVTXW: %s\n", buf);
```

---

## 15. Core Interface Example {#NVTXW_CORE_EXAMPLE}

The following example uses the reference loader only to obtain the interface table. After that, it
uses core NVTXW function-table entries directly: create a session, register a domain, open a stream,
register a counter, write one counter sample, and finalize the session.

```c
#include <nvtx3/nvToolsExt.h>
#include <nvtxw3/nvtxw3.h>
#include <nvtxw3/nvtxw3_loader.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(const char* what, nvtxwResultCode_t rc)
{
    char buf[128];
    /* nvtxwGetError is provided by the reference loader. */
    nvtxwGetError(rc, buf, sizeof(buf));
    fprintf(stderr, "%s failed: %s\n", what, buf);
    return 1;
}

int main(void)
{
    nvtxwGetInterface_t getInterface = NULL;
    nvtxwResultCode_t rc = nvtxwLoad(NULL, &getInterface, NULL);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("load", rc);

    const void* ifacePtr = NULL;
    rc = getInterface(NVTXW_INTERFACE_VERSION, &ifacePtr);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("nvtxwGetInterface", rc);

    const nvtxwInterface_t* iface = (const nvtxwInterface_t*)ifacePtr;

    nvtxwSessionAttributes_t sessionAttr;
    memset(&sessionAttr, 0, sizeof(sessionAttr));
    sessionAttr.structSize = sizeof(sessionAttr);
    sessionAttr.name = "core-demo";

    nvtxwSessionHandle_t session = NULL;
    rc = iface->SessionBegin(&sessionAttr, &session);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("SessionBegin", rc);

    nvtxwDomainAttributes_t domainAttr;
    memset(&domainAttr, 0, sizeof(domainAttr));
    domainAttr.structSize = sizeof(domainAttr);
    domainAttr.name = "demo-domain";

    nvtxDomainHandle_t domain = NULL;
    rc = iface->DomainRegister(session, &domainAttr, &domain);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("DomainRegister", rc);

    nvtxwStreamAttributes_t streamAttr;
    memset(&streamAttr, 0, sizeof(streamAttr));
    streamAttr.structSize = sizeof(streamAttr);
    streamAttr.name = "demo-stream";
    streamAttr.domain = domain;

    nvtxwStreamHandle_t stream = NULL;
    rc = iface->StreamOpen(session, &streamAttr, &stream);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("StreamOpen", rc);

    nvtxCounterAttr_t counterAttr;
    memset(&counterAttr, 0, sizeof(counterAttr));
    counterAttr.structSize = sizeof(counterAttr);
    counterAttr.schemaId = NVTX_PAYLOAD_ENTRY_TYPE_INT64;
    counterAttr.name = "queue-depth";

    uint64_t counterId = NVTX_COUNTER_ID_NONE;
    rc = iface->CounterRegister(domain, &counterAttr, &counterId);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("CounterRegister", rc);

    int64_t value = 42;
    rc = iface->CounterWrite(stream, /* timestamp */ 100, counterId, &value, sizeof(value));
    if (rc != NVTXW_RESULT_SUCCESS) return fail("CounterWrite", rc);

    iface->StreamClose(stream);
    rc = iface->SessionEnd(session);
    if (rc != NVTXW_RESULT_SUCCESS) return fail("SessionEnd", rc);

    return 0;
}
```

For event examples, see [Writing Events](#NVTXW_WRITING_EVENTS).

---

## 16. Tips and Gotchas

- **Register before reference.** Register schemas, scopes, counters, strings, and time domains
  before any data that uses them.
- **Provide timestamps yourself.** NVTXW does not capture time for you. A time domain is optional.
  Without one, the tool uses a relative timeline. Register a known time domain when the tool must
  correlate or merge your timestamps with another clock.
- **`SessionEnd` is the flush.** Closing a stream does not signal "no more data"; only `SessionEnd`
  does.
- **Use registered strings for repeated messages.** Prefer `StringRegister` +
  `nvtxwEventAttributes_t` over re-sending the same UTF-8 string.
- **Share one domain across streams efficiently.** Register schemas once
  (`nvtxwEventSchemasRegister`), build one writer, and copy it per stream rather than
  re-registering.
- **Check optional members.** Use `NVTXW_INTERFACE_HAS` before calling members that newer minor
  versions may have added, in case the backend is older.
- **Redirect the backend without rebuilding.** Set `NVTXW3_LIBRARY` to point the loader at a
  specific backend library.
- **Be conservative with backend paths.** If users can provide a backend path, require an absolute
  path or otherwise validate it before loading.
- **One stream per worker.** A stream maps naturally onto a logical worker such as a thread, process,
  rank, or GPU stream. Because streams are independent, you can write to each from its own thread
  without locking (see [Thread Safety](#NVTXW_THREAD_SAFETY)).

<!--
SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

# Extended Payloads: User Guide {#NVTX_EXTENDED_PAYLOADS}

\tableofcontents

This guide shows how to attach arbitrary data to NVTX mark and range events using the
**payload extension**. A payload can be as simple as a single scalar value or a raw byte
blob, or as rich as a schema-described composition of typed fields: nested structs,
unions, enums, fixed- or variable-length arrays, and strings. The extension also covers
deferred event submission and custom time domains.

The API is declared in `nvtx3/nvToolsExtPayload.h`. Convenience macros for describing C
structs and schemas side-by-side live in `nvtx3/nvToolsExtPayloadHelper.h`.

## Concepts at a Glance {#NVTX_EXTENDED_PAYLOADS_CONCEPTS}

- **Payload data** (@ref nvtxPayloadData_t): a byte blob with its size and a schema ID
  that tells tools how to decode it.
- **Schema**: an array of @ref nvtxPayloadSchemaEntry_t describing how payload bytes map
  to fields, registered with @ref nvtxPayloadSchemaRegister and referenced by a
  **schema ID**.
- **Scope**: an identifier for *where* an event happened (CPU core, device, process,
  thread, user-defined context, ...). Registered via @ref nvtxScopeRegister, with
  predefined values (`NVTX_SCOPE_*`) and optional parent-scope hierarchies.
- **Deferred event**: an event whose timestamp(s) are supplied by the application
  instead of the tool. Submitted via @ref nvtxEventSubmit or, in batches, via
  @ref nvtxEventBatch_t.
- **Semantics**: optional metadata attached to a schema entry that refines how a tool
  interprets it, for example marking an entry as a timestamp from a specific time
  domain, a counter with a unit, a scope, or a correlation ID. Carried in the entry's
  @ref nvtxPayloadSchemaEntry_t::semantics field.

All registered IDs (schemas, scopes, time domains, ...) are **per NVTX
domain**.

## When to Use Extended Payloads {#NVTX_EXTENDED_PAYLOADS_WHEN}

Classic NVTX events carry a message and optionally a category, a color, and a single
numeric payload field (a scalar of a predefined type such as `int64_t` or `double`).
Extended payloads let you attach **arbitrary data** to a mark or range event. The data
can be:

- a blob described by a **registered schema** that tells tools how to decode it into
  typed fields (nested structs, unions, enums, arrays, strings, ...),
- a **raw blob** (@ref NVTX_TYPE_PAYLOAD_SCHEMA_RAW) that a tool may display
  with a hex/binary viewer when no schema is available, or
- a single value of a **predefined type** (e.g. an `int`, a `double`, or a C-string),
  decoded without a schema. (Note that for a single numeric scalar, the classic payload
  field on @ref nvtxEventAttributes_v2 "nvtxEventAttributes_t" might be simpler.)

Typical use cases:

- Attach function parameters or other structured data to a range or mark.
- Submit previously recorded events from any source (device, other process,
  log file, in-memory buffer) as **deferred events** with their original
  timestamps.

Extended payloads are also a practical alternative to encoding parameters or other
structured data as strings, for example in the event message or as JSON. This avoids
the formatting cost of `sprintf`-like functions on the critical path. Schemas are
registered once, so event emission mostly just passes typed data blobs to NVTX,
keeping annotation overhead low.

## Minimal Example {#NVTX_EXTENDED_PAYLOADS_MINIMAL}

Define a payload struct together with its schema using the helper macros in
`nvtx3/nvToolsExtPayloadHelper.h`, register the schema once, then wrap some work in a
push/pop range that carries a typed payload:

```c
#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtPayloadHelper.h>

/* Declare `iteration_t` and a matching schema description with helper macro. */
NVTX_DEFINE_STRUCT_WITH_SCHEMA(iteration_t, "example.iteration",
    NVTX_PAYLOAD_ENTRIES(
        (uint64_t, index,     TYPE_UINT64, "Iteration index"),
        (uint32_t, itemCount, TYPE_UINT32, "Items processed")
    )
)

/* One-time setup. */
uint64_t schemaId = NVTX_PAYLOAD_SCHEMA_REGISTER(domain, iteration_t);
nvtxStringHandle_t msgHdl = nvtxDomainRegisterStringA(domain, "iteration");
nvtxEventAttributes_t attr = {0};
attr.version            = NVTX_VERSION;
attr.size               = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
attr.messageType        = NVTX_MESSAGE_TYPE_REGISTERED;
attr.message.registered = msgHdl;

/* Per iteration. */
iteration_t value = {index, itemCount};
nvtxPayloadRangePush(domain, &attr, schemaId, &value, sizeof(value));
/* ... do the iteration's work here ... */
nvtxDomainRangePop(domain);
```

## Workflow {#NVTX_EXTENDED_PAYLOADS_WORKFLOW}

1. Define the payload layout: either as a C `struct` or as an explicit byte layout.
2. Describe each field as an @ref nvtxPayloadSchemaEntry_t.
3. Fill @ref nvtxPayloadSchemaAttr_t (at minimum `type`, `entries`, `numEntries`; set
   `payloadStaticSize` for static schemas) and register it with
   @ref nvtxPayloadSchemaRegister. Keep the returned schema ID.
4. Optionally register enums (@ref nvtxPayloadEnumRegister), scopes
   (@ref nvtxScopeRegister), and time domains (@ref nvtxTimeDomainRegister).
5. Build one or more @ref nvtxPayloadData_t values pointing to the payload bytes.
6. Emit events either by
   [attaching the payload to `nvtxEventAttributes_t`](#NVTX_EXTENDED_PAYLOADS_ATTACH)
   and calling an attribute-based NVTX API, or by using one of the
   [dedicated payload event APIs](#NVTX_EXTENDED_PAYLOADS_EVENT_APIS) such as
   @ref nvtxMarkPayload or @ref nvtxRangePushPayload.

A single event can carry multiple payloads by passing an array of @ref nvtxPayloadData_t.
This is how you combine a primary payload (describing the event) with auxiliary
payloads (for example, a referenced data blob that other entries point into).

## Schemas {#NVTX_EXTENDED_PAYLOADS_SCHEMAS}

A schema is registered by filling in an @ref nvtxPayloadSchemaAttr_t and passing it to
@ref nvtxPayloadSchemaRegister. The attributes are:

- `type` (mandatory) selects the structural kind of the schema (see
  [Types](#NVTX_EXTENDED_PAYLOADS_SCHEMA_ATTR_TYPE)).
- `entries` and `numEntries` (mandatory) describe the individual fields.
- `flags` (optional) annotate the role of the schema and hints about its
  contents (see [Flags](#NVTX_EXTENDED_PAYLOADS_SCHEMA_ATTR_FLAGS)).
- `payloadStaticSize` is the total size in bytes of a static payload; set it to
  `sizeof(YourStruct)` so tools can decode deterministically.
- `packAlign` caps member alignment just like C's `#pragma pack(N)`; leave it at `0`
  for natural alignment.
- `schemaId` optionally assigns a stable, user-chosen ID; otherwise the NVTX
  handler returns a generated one from @ref nvtxPayloadSchemaRegister.
- `name` is an optional human-readable label shown by tools.
- `fieldMask` must include the `NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_*` bit for every field
  that has been populated; at minimum `TYPE`, `ENTRIES`, and `NUM_ENTRIES` must be
  set.

### Types {#NVTX_EXTENDED_PAYLOADS_SCHEMA_ATTR_TYPE}

Set @ref nvtxPayloadSchemaAttr_t::type to one of:

- @ref NVTX_PAYLOAD_SCHEMA_TYPE_STATIC - fixed-size C-like struct. All entry offsets
  and sizes are known at registration time. Set `payloadStaticSize` to
  `sizeof(YourStruct)` whenever possible so tools can decode deterministically.
  Variable-length embedded fields are not allowed.
- @ref NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC - variable-length payload. Tools walk the
  entries sequentially, advancing a cursor with natural alignment. Use this when the
  payload contains variable-length arrays or strings embedded in the blob.
- @ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION - C-like union whose active member is
  selected by a separate entry (index given by `arrayOrUnionDetail`).
- @ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR - union whose selector
  entry is part of the same schema (entry of type
  @ref NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR).

### Flags {#NVTX_EXTENDED_PAYLOADS_SCHEMA_ATTR_FLAGS}

@ref nvtxPayloadSchemaAttr_t::flags falls into three independent groups and can be
combined with bitwise OR:

- Event-role flags describe what kind of event the schema represents so a tool knows
  how to interpret the timestamps and message it contains:
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_MARK (instantaneous marker),
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP (push/pop range),
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND (start/end range), and
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_COUNTER_GROUP (group of counter values). These
  are mutually exclusive; at most one should be set. For counter registration and
  sampling, use `nvtx3/nvToolsExtCounters.h`.
- @ref NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY signals that the payload contains fields
  (for example pointers to strings or arrays) that the NVTX handler must deep-copy to
  preserve the data beyond the call. It is a hint about payload contents, not about
  the event's role.
- @ref NVTX_PAYLOAD_SCHEMA_FLAG_REFERENCED signals that payloads of this schema are
  meant to be referenced by other payloads of the same event rather than visualized on
  their own. If the schema is never visualized directly,
  @ref NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED can be used instead.

### Entry Semantics {#NVTX_EXTENDED_PAYLOADS_SEMANTICS}

Beyond its type and name, a schema entry can carry optional *semantics* that refine
how a tool interprets it, for example marking an `int64_t` as a timestamp from a
specific time domain or a value as a counter with a unit. Semantics are evaluated at
registration time, so their values must be known then; use a payload entry when a
value must vary per event.

The @ref nvtxPayloadSchemaEntry_t::semantics field points to the first element of a
linked list of extension structs, each beginning with an @ref nvtxSemanticsHeader_t
(`structSize`, `semanticId`, `version`, and a `next` pointer). Chaining structs
through `next` lets one entry carry several semantics at once. Each extension lives in
its own header:

- **Scope** (@ref nvtxSemanticsScope_t) - the execution scope of the entry; see
  [Scopes](#NVTX_EXTENDED_PAYLOADS_SCOPES).
- **Time** (@ref nvtxSemanticsTime_t) - the time domain of a timestamp entry; see
  [Time Domains](#NVTX_EXTENDED_PAYLOADS_TIME_DOMAINS).
- **Correlation** (@ref nvtxSemanticsCorrelation_t) - marks the entry as a correlation
  ID that links events sharing the same correlation domain.
- **Counter** (@ref nvtxSemanticsCounter_t) - unit, scale, limits, and graphing flags
  for a counter entry (flagged with @ref NVTX_PAYLOAD_ENTRY_FLAG_COUNTER). For
  standalone counter registration and sampling, use the dedicated counter extension
  in `nvtx3/nvToolsExtCounters.h`.

For a code example that attaches time and scope semantics to timestamp entries, see
[Deferred marks and ranges](#NVTX_EXTENDED_PAYLOADS_HOWTO_DEFERRED).

## Attaching Payloads to Event Attributes {#NVTX_EXTENDED_PAYLOADS_ATTACH}

Attach an array of `nvtxPayloadData_t` to an
@ref nvtxEventAttributes_v2 "nvtxEventAttributes_t" and emit the event through any of
the attribute-based NVTX APIs (for example
`nvtxDomainRangePushEx` or `nvtxDomainMarkEx`).

For the single-payload case, the helper macros @ref nvtxPayloadRangePush and
@ref nvtxPayloadMark combine attribute setup and emission:

```c
nvtxEventAttributes_t attr = {0};
attr.version = NVTX_VERSION;
attr.size    = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
attr.messageType   = NVTX_MESSAGE_TYPE_ASCII;
attr.message.ascii = "training-step";

nvtxPayloadRangePush(domain, &attr, schemaId, &value, sizeof(value));
```

For multiple payloads, the helper macro @ref NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE can be
used to bind an `nvtxPayloadData_t` array to
@ref nvtxEventAttributes_v2 "nvtxEventAttributes_t":

```c
nvtxPayloadData_t payloads[] = {
    {stepSchemaId,   sizeof(step),   &step},
    {tensorSchemaId, sizeof(tensor), &tensor},
};

nvtxEventAttributes_t attr = {0};
attr.version = NVTX_VERSION;
attr.size    = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE(attr, payloads);
nvtxDomainMarkEx(domain, &attr);
```

## Payload Event APIs {#NVTX_EXTENDED_PAYLOADS_EVENT_APIS}

The payload extension also adds dedicated event APIs that take `nvtxPayloadData_t`
directly, without going through `nvtxEventAttributes_t`:

- @ref nvtxMarkPayload
- @ref nvtxRangePushPayload / @ref nvtxRangePopPayload
- @ref nvtxRangeStartPayload / @ref nvtxRangeEndPayload
- @ref nvtxEventSubmit / @ref nvtxEventBatchSubmit (deferred submission)

Each takes a pointer to an `nvtxPayloadData_t` array and the number of payloads:

```c
nvtxPayloadData_t pld = {schemaId, sizeof(value), &value};
nvtxRangePushPayload(domain, &pld, 1);
```

Event attributes must be supplied as payload entries. If the event message is missing
(see @ref NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE), the tool may ignore the event.

## Event Attribute Precedence {#NVTX_EXTENDED_PAYLOADS_EVENT_ATTRIBUTE_PRECEDENCE}

If the same event attribute is specified more than once for a logical event, the
latest-specified value is the event's effective value. A tool may also preserve earlier
values for deeper inspection, but applications should not rely on those superseded
values being available.

For a range, attributes supplied on the end/pop side are later than attributes supplied
on the start/push side. For example, if a range begins with color green and ends with
color red, the effective range color is red.

Within a single API call, event attributes are applied in this order:

1. Regular @ref nvtxEventAttributes_v2 "nvtxEventAttributes_t" members, such as message
   or color, if the API takes event attributes.
2. Extended payload entries, walking the `nvtxPayloadData_t` array in order.
3. For each payload, schema entries in order.

For ranges, tools that act before the range is complete can only use the effective
attributes known at that time. For example, a tool that starts collection when a range
is pushed cannot use attributes that will only be supplied later by
@ref nvtxRangePopPayload or @ref nvtxRangeEndPayload. This can affect runtime filtering
and triggering in tools that support it, so applications are recommended to provide
event attribute values as early as possible.

Tools are not required to support runtime filtering or triggering on event attributes
specified in an extended payload, because decoding payload schemas at runtime may add
overhead.

## Strings {#NVTX_EXTENDED_PAYLOADS_STRINGS}

To explicitly support C strings, use one of the @ref NVTX_PAYLOAD_ENTRY_TYPE_CSTRING
types or @ref NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE. Strings can be
put into the payload in several ways:

- **Registered string handle:** Set the entry type to
  @ref NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE. This is recommended for
  strings that are reused many times to reduce storage cost and copy overhead, and is
  often the best choice for medium or longer labels (for example, 8+ characters).
- **Fixed-size embedded string:** Set `arrayOrUnionDetail > 0` on a `CSTRING*` entry.
  The length is counted in string code units, not bytes or Unicode code points:
  1 byte for `CSTRING`/`CSTRING_UTF8`, 2 bytes for `CSTRING_UTF16`, and 4 bytes for
  `CSTRING_UTF32`.
  If the string is null-terminated before the declared length, a tool will display
  only the data up to the null terminator. This is often best for very short strings.
  Setting @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE on such an entry is redundant;
  it still denotes a single fixed-length string of `arrayOrUnionDetail` code units.
  A fixed-size array of fixed-size strings therefore cannot be described directly, because
  `arrayOrUnionDetail` already describes the string length in code units. Define a
  nested schema for one fixed-size string and then use that schema as the array
  element type.
- **Standalone string payload:** Put the string bytes in a separate payload of the same
  event. The `schemaId` of `nvtxPayloadData_t` can either be a schema whose only entry
  is a string (useful when the payload should still have a label) or a predefined
  string type such as @ref NVTX_PAYLOAD_ENTRY_TYPE_CSTRING. The `payload` field of
  `nvtxPayloadData_t` points to the first character of the string. If a referenced
  payload is used, another payload entry in the same event can point to it via
  @ref NVTX_PAYLOAD_ENTRY_FLAG_POINTER. If `size` is an explicit finite value, it
  bounds the string bytes (at least one code unit) and the payload does not need to
  include a null terminator. If `size == SIZE_MAX`, the string must be null-terminated
  so the tool can determine its length.
- **Variable-length embedded string:** Use a dynamic schema and either
  @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED or
  @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX on a `CSTRING*` entry. In C, a
  trailing flexible array member is a typical implementation for this layout. For
  length-indexed strings, the referenced length entry stores a string code-unit count.
- **Pointer to string data in another payload of the same event:** Set
  @ref NVTX_PAYLOAD_ENTRY_FLAG_POINTER on the string entry. If
  `arrayOrUnionDetail == 0`, a null-terminated string is expected; otherwise, exactly
  `arrayOrUnionDetail` string code units are read from the pointed-to address.
  As with embedded fixed-size strings, if a null terminator occurs before the
  declared length, a tool will use only the data up to the null terminator.
- **Pointer to external string data (deep copy):** Use a pointer-form string entry and
  set @ref NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY on the entry and
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY on the schema. If `arrayOrUnionDetail == 0`,
  a null-terminated string is expected; otherwise, exactly `arrayOrUnionDetail`
  string code units are read starting from the pointed-to address. As with embedded
  fixed-size strings, if a null terminator occurs before the declared length, a tool
  will use only the data up to the null terminator. This is typically
  a high-overhead option because the tool may need to inspect the schema at event
  time, allocate storage, and copy data.

### Empty strings {#NVTX_EXTENDED_PAYLOADS_EMPTY_STRINGS}

An empty string is always representable. It is a *present, zero-length* value,
distinct from an *absent* one: a `NULL` pointer in a pointer-form string entry
means absent (rendered, for example, as `<null>`), not empty.

The encoding depends on the mechanism:

- **Standalone string payload:** a single null code unit (`size` is one code unit,
  `payload` points at `""`).
- **Fixed-size embedded string:** a capacity of at least one code unit
  (`arrayOrUnionDetail >= 1`) whose first code unit is null. `arrayOrUnionDetail == 0`
  is the pointer/null-terminated form, not an empty string.
- **Zero-terminated embedded string:** just the terminator.
- **Length-indexed embedded string:** a length of 0.
- **Pointer-form string:** a pointer to `""`.

## Event Messages {#NVTX_EXTENDED_PAYLOADS_EVENT_MESSAGES}

Event messages can be provided in two places:

- For attribute-based APIs such as @ref nvtxDomainMarkEx or @ref nvtxDomainRangePushEx,
  use the regular `nvtxEventAttributes_t` message fields (`messageType` and
  `message.*`).
- For payload event APIs such as @ref nvtxMarkPayload, @ref nvtxRangePushPayload,
  @ref nvtxEventSubmit, and deferred events, mark a payload string entry with
  @ref NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE.

For marks and ranges emitted via payload APIs or deferred submission, provide an
explicit payload event-message entry when the display name matters. If it is missing,
a tool may ignore the event.

Applications are recommended to provide only one logical event message per event. For
ranges, provide the message on the push/start side and keep it stable so runtime
filtering is possible. Use color or any other field in the extended payload to indicate
a change in state.

If an event intentionally supplies multiple messages, the effective message is selected
by the ordering described in
[Event Attribute Precedence](#NVTX_EXTENDED_PAYLOADS_EVENT_ATTRIBUTE_PRECEDENCE).

A tool is free to display multiple supplied messages concatenated or otherwise combined
as a presentation choice.

```c
nvtxStringHandle_t msg = nvtxDomainRegisterStringA(domain, "training-step");

const nvtxPayloadSchemaEntry_t entries[] = {
    {NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE,
     NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE, "message"},
    {0, NVTX_PAYLOAD_ENTRY_TYPE_UINT32, "iteration"}
};

typedef struct {
    nvtxStringHandle_t msg;
    uint32_t iteration;
} StepPayload_t;

StepPayload_t step = {msg, 42};
nvtxPayloadData_t pld = {schemaId, sizeof(step), &step};
nvtxMarkPayload(domain, &pld, 1);
```

## Deferred Events {#NVTX_EXTENDED_PAYLOADS_DEFERRED}

Extended payloads are also the mechanism for submitting events produced ahead of time
(for example, by a device or another process). Deferred events can include timestamps
and scope information that describe when and where the event originated; see
[Scopes](#NVTX_EXTENDED_PAYLOADS_SCOPES) for how the scope is specified.

- @ref nvtxTimestampGet - obtain a tool-provided timestamp suitable for use
  in deferred events.
- @ref nvtxEventSubmit - submit a single deferred mark or range.
- @ref nvtxEventBatchSubmit - submit a batch of deferred events using
  @ref nvtxEventBatch_t.

A deferred event's schema typically has one of the `NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_*`
flags (for example @ref NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP or
@ref NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND) or @ref NVTX_PAYLOAD_SCHEMA_FLAG_MARK set,
and carries at least an event-message entry.

## Scopes {#NVTX_EXTENDED_PAYLOADS_SCOPES}

Scopes describe where an event or counter originated, such as a process, thread,
device, queue, or user-defined execution context.

Events and counters can specify scope in several ways, including a scope payload entry
and scope [semantics](#NVTX_EXTENDED_PAYLOADS_SEMANTICS) (@ref nvtxSemanticsScope_t).
Since a payload can describe different event/counter roles and timestamp purposes, an
event may also carry multiple scopes for different purposes. For the scope sources and
precedence rules, see @ref NVTX_SCOPE_SPECIFICATION_AND_PRECEDENCE.

When possible, prefer one scope for each event role, counter role, or timestamp purpose
to keep tool behavior predictable.

The predefined `NVTX_SCOPE_CURRENT_*` values are runtime-resolved: a tool resolves them
against the live execution context when an event or counter sample is taken. Scopes
registered with @ref nvtxScopeRegister are stable identities instead. Because a stable
identity cannot embed a per-sample parent, @ref nvtxScopeAttr_t::parentScope must be
`NVTX_SCOPE_ROOT`, `NVTX_SCOPE_NONE`, or another registered scope ID for the same
domain, not a `NVTX_SCOPE_CURRENT_*` value. To anchor a registered scope to an execution
context, register a scope for that context and use its ID as the parent.

Use @ref NVTX_SCOPE_NONE when the original execution scope is unknown, intentionally
unspecified, or not applicable. Deferred events with @ref NVTX_SCOPE_NONE should not be
attributed to the thread or process that submits them.

Scopes for different purposes are independent. For example, a
@ref NVTX_PAYLOAD_ENTRY_TYPE_SCOPE_ID entry with @ref NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN
can specify where the range begins, while another scope entry with
@ref NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP can specify the scope associated with the timestamp
source. Those scopes may be different.

For a single deferred event submitted with @ref nvtxEventSubmit, there is no batch-level
scope field. Prefer scope semantics when the scope is known at schema registration time,
because this avoids adding a scope field to each event payload. Use a scope entry in the
payload when the scope must vary per event.

For deferred batches, @ref nvtxEventBatch_t::scope is a submit-time default: it
overrides general scope semantics from the schema, but not a scope entry or
purpose-specific scope semantics for an event role, counter role, or timestamp purpose.

@ref nvtxTimeDomainAttr_t::scopeId is related but separate: it associates a timestamp
source with a scope so tools can interpret timestamps from that time domain. It does
not override a more specific event-attribution scope. If no other event scope is
specified, a tool may also use the timestamp scope as the event's general scope.

## Time Domains {#NVTX_EXTENDED_PAYLOADS_TIME_DOMAINS}

Time domains describe the clock source used by deferred timestamps and can associate
that timestamp source with a scope via @ref nvtxTimeDomainRegister. A timestamp entry
selects its time domain through time [semantics](#NVTX_EXTENDED_PAYLOADS_SEMANTICS)
(@ref nvtxSemanticsTime_t).

A predefined `NVTX_TIMESTAMP_TYPE_*` value can be used directly as a time domain ID
when it unambiguously identifies the timestamp source. Register a time domain when
multiple timestamp sources share the same predefined type, or when the source needs
additional metadata such as scope, resolution, or epoch.

- @ref nvtxTimeDomainRegister - declare a time domain (clock source, scope, resolution,
  epoch).
- @ref nvtxTimerSource / @ref nvtxTimerSourceWithData - provide a function pointer the
  tool can call to sample the timer.
- @ref nvtxTimeSyncPoint / @ref nvtxTimeSyncPointTable /
  @ref nvtxTimestampConversionFactor - provide synchronization between time domains so
  tools can convert timestamps.

## How-To Examples {#NVTX_EXTENDED_PAYLOADS_HOWTO}

This section contains a non-exhaustive list of example usages of NVTX extended payloads.

### Fixed-size embedded arrays and strings {#NVTX_EXTENDED_PAYLOADS_FIXED_ARRAYS}

Use @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE and set `arrayOrUnionDetail` to
the element count. For embedded fixed-size strings (C-string type with a positive
`arrayOrUnionDetail`), this value is a string code-unit count and the array flag is
redundant.

```c
typedef struct
{
    char     tag[24];      /* inline fixed-size string (24 chars) */
    float    coords[3];    /* inline fixed-size float[3] */
} FixedShape_t;

static const nvtxPayloadSchemaEntry_t entries[] = {
    {0, NVTX_PAYLOAD_ENTRY_TYPE_CSTRING, "tag", NULL, 24},
    {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE,
     NVTX_PAYLOAD_ENTRY_TYPE_FLOAT, "coords", NULL, 3}
};
```

### Variable-length arrays {#NVTX_EXTENDED_PAYLOADS_VAR_ARRAYS}

Use @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX with `arrayOrUnionDetail` set to
the **index** of a prior entry holding the length. The array is embedded in the payload
bytes. This requires a @ref NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC schema.

```c
/* Layout: uint32_t n; float values[n]; */
static const nvtxPayloadSchemaEntry_t entries[] = {
    {0, NVTX_PAYLOAD_ENTRY_TYPE_UINT32, "n"},
    {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX,
     NVTX_PAYLOAD_ENTRY_TYPE_FLOAT, "values", NULL, /*length index*/0}
};

nvtxPayloadSchemaAttr_t attr = {0};
attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC;
/* ...entries, numEntries, flags... */
```

When you already have a pointer to array data (for example, an existing `float*`
buffer) and want to avoid copying, emit the length in one payload and the array bytes
in another, and use @ref NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX with
`arrayOrUnionDetail` set to the **payload index** holding the length:

```c
const nvtxPayloadSchemaEntry_t dataSchema[] = {
    {NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_PAYLOAD_INDEX,
     NVTX_PAYLOAD_ENTRY_TYPE_FLOAT, "data", NULL, /*payload index*/0}
};

nvtxPayloadSchemaAttr_t attr = {0};
attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC;
/* ...attr.entries = dataSchema; attr.numEntries = 1; attr.flags... */
uint64_t dataSchemaId = /* nvtxPayloadSchemaRegister(domain, &attr) */;

size_t count = 128;
float* myFloatPtr = /* ... */;

nvtxPayloadData_t payloads[] = {
    {NVTX_PAYLOAD_ENTRY_TYPE_SIZE, sizeof(size_t), &count},
    {dataSchemaId, count * sizeof(float), myFloatPtr}
};
```

### Deep-copy of strings and buffers {#NVTX_EXTENDED_PAYLOADS_DEEP_COPY}

When an entry holds a pointer to memory outside the payloads of the event, set
@ref NVTX_PAYLOAD_ENTRY_FLAG_DEEP_COPY on the entry **and**
@ref NVTX_PAYLOAD_SCHEMA_FLAG_DEEP_COPY on the schema. Tools that support deep copy
will copy the referenced memory; tools that do not may ignore the flag.

### Nested schemas {#NVTX_EXTENDED_PAYLOADS_NESTED}

Use a previously registered schema ID in place of a predefined type to embed a nested
struct inline. The tool takes size and alignment from the referenced schema.

```c
typedef struct {
    float x;
    float y;
} Point_t;

typedef struct {
    int16_t flag;
    Point_t nestedStruct;
} Outer_t;

const nvtxPayloadSchemaEntry_t inner[] = {
    {0, NVTX_PAYLOAD_ENTRY_TYPE_FLOAT, "x"},
    {0, NVTX_PAYLOAD_ENTRY_TYPE_FLOAT, "y"}
};

nvtxPayloadSchemaAttr_t innerAttr = {0};
/* ...innerAttr.fieldMask, type, entries, numEntries ... */
uint64_t innerSchemaId = nvtxPayloadSchemaRegister(domain, &innerAttr);

const nvtxPayloadSchemaEntry_t outer[] = {
    {0, NVTX_PAYLOAD_ENTRY_TYPE_INT16, "flag"},
    {0, innerSchemaId, "nestedStruct"}
};

nvtxPayloadSchemaAttr_t outerAttr = {0};
/* ...outerAttr.fieldMask, type, entries, numEntries ... */
uint64_t outerSchemaId = nvtxPayloadSchemaRegister(domain, &outerAttr);
```

### Enums {#NVTX_EXTENDED_PAYLOADS_ENUMS}

Describe C-like enums with @ref nvtxPayloadEnumAttr_t and register them with
@ref nvtxPayloadEnumRegister. Set `sizeOfEnum` to the storage size of the enum.

```c
typedef enum { OP_ADD = 0, OP_MUL = 1, OP_DIV = 2 } Op_t;

/* Last field is isFlag; use 1 for bitset-style enum entries. */
const nvtxPayloadEnum_t values[] = {
    {"OP_ADD", OP_ADD, 0},
    {"OP_MUL", OP_MUL, 0},
    {"OP_DIV", OP_DIV, 0},
};

nvtxPayloadEnumAttr_t enumAttr = {0};
/* ...enumAttr.fieldMask, name, entries, numEntries, sizeOfEnum ... */
const uint64_t opEnumId = nvtxPayloadEnumRegister(domain, &enumAttr);
```

Use the returned enum ID as the `type` of a schema entry.

### Unions {#NVTX_EXTENDED_PAYLOADS_UNIONS}

For a union with an **external** selector, add an integer-typed selector entry and
reference its index from the union-typed entry via `arrayOrUnionDetail`. For an
**internal** selector, add an entry of type @ref NVTX_PAYLOAD_ENTRY_TYPE_UNION_SELECTOR
inside the union schema and register it as
@ref NVTX_PAYLOAD_SCHEMA_TYPE_UNION_WITH_INTERNAL_SELECTOR.

### Deferred marks and ranges {#NVTX_EXTENDED_PAYLOADS_HOWTO_DEFERRED}

Use deferred marks and ranges when the event timestamp is already known, for example
for work recorded on a device, in another process, or after the event has already
happened. Tools use the supplied timestamp fields to place the event on the timeline at
its original time, rather than at the time the event is submitted. The payload schema
describes both the event data and the timestamp fields.

For a deferred event schema:

- Set the event role with @ref NVTX_PAYLOAD_SCHEMA_FLAG_MARK,
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP, or
  @ref NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_STARTEND.
- Add timestamp entries, typically @ref NVTX_PAYLOAD_ENTRY_TYPE_INT64, with
  @ref NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP plus the event-specific flag:
  @ref NVTX_PAYLOAD_ENTRY_FLAG_MARK for marks, or
  @ref NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN and @ref NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END
  for ranges.
- Add a string entry with @ref NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE to name the event.

The example below registers the scope where the deferred ranges originated and submits
three deferred ranges in one batch. The timestamp entries use time `semantics` with the
predefined TSC timestamp type as the time domain, and link to scope `semantics` for the
execution scope.

```c
#include <nvtx3/nvToolsExtSemanticsScope.h>
#include <nvtx3/nvToolsExtSemanticsTime.h>

/* Minimal deferred range payload. */
typedef struct {
    int64_t beginTime;
    int64_t endTime;
    nvtxStringHandle_t msg;
} DeferredRange_t;

/* Register the original execution scope of the deferred events. */
nvtxScopeAttr_t scopeAttr = {0};
scopeAttr.structSize = sizeof(nvtxScopeAttr_t);
scopeAttr.path = "device/queue";
uint64_t scopeId = nvtxScopeRegister(domain, &scopeAttr);

/* Create scope semantics. */
nvtxSemanticsScope_t scopeSemantic = {0};
scopeSemantic.header.structSize = sizeof(nvtxSemanticsScope_t);
scopeSemantic.header.semanticId = NVTX_SEMANTIC_ID_SCOPE_V1;
scopeSemantic.header.version = NVTX_SCOPE_SEMANTIC_VERSION;
scopeSemantic.header.next = NULL;
scopeSemantic.scopeId = scopeId;

/* Create time semantics and link the scope semantics. */
nvtxSemanticsTime_t timeSemantic = {0};
timeSemantic.header.structSize = sizeof(nvtxSemanticsTime_t);
timeSemantic.header.semanticId = NVTX_SEMANTIC_ID_TIME_V1;
timeSemantic.header.version = NVTX_TIME_SEMANTIC_VERSION;
timeSemantic.header.next = (const nvtxSemanticsHeader_t*)&scopeSemantic;
/* A predefined timestamp type may be used as the time domain if unambiguous. */
timeSemantic.timeDomainId = NVTX_TIMESTAMP_TYPE_CPU_TSC;

/* Create schema with time and scope semantics for the timestamp entries. */
const nvtxPayloadSchemaEntry_t entries[] = {
    {NVTX_PAYLOAD_ENTRY_FLAG_RANGE_BEGIN | NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP,
     NVTX_PAYLOAD_ENTRY_TYPE_INT64, "beginTime", NULL, 0, 0,
     (const nvtxSemanticsHeader_t*)&timeSemantic},
    {NVTX_PAYLOAD_ENTRY_FLAG_RANGE_END | NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP,
     NVTX_PAYLOAD_ENTRY_TYPE_INT64, "endTime", NULL, 0, 0,
     (const nvtxSemanticsHeader_t*)&timeSemantic},
    {NVTX_PAYLOAD_ENTRY_FLAG_EVENT_MESSAGE,
     NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE, "msg"}
};

nvtxPayloadSchemaAttr_t attr = {0};
attr.fieldMask = NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_TYPE |
                 NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_FLAGS |
                 NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_ENTRIES |
                 NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_NUM_ENTRIES |
                 NVTX_PAYLOAD_SCHEMA_ATTR_FIELD_STATIC_SIZE;
attr.type = NVTX_PAYLOAD_SCHEMA_TYPE_STATIC;
attr.flags = NVTX_PAYLOAD_SCHEMA_FLAG_RANGE_PUSHPOP;
attr.entries = entries;
attr.numEntries = sizeof(entries) / sizeof(entries[0]);
attr.payloadStaticSize = sizeof(DeferredRange_t);
const uint64_t schemaId = nvtxPayloadSchemaRegister(domain, &attr);

/* Submit three ranges from the same scope. */
int64_t beginTsc[3] = { /* ... */ };
int64_t endTsc[3] = { /* ... */ };
nvtxStringHandle_t msg = nvtxDomainRegisterStringA(domain, "device work");

DeferredRange_t ranges[3];
for (size_t i = 0; i < 3; ++i) {
    ranges[i].beginTime = beginTsc[i];
    ranges[i].endTime = endTsc[i];
    ranges[i].msg = msg;

    /* Or submit each range immediately (without batch.scope):
    nvtxPayloadData_t payload[] = {{schemaId, sizeof(ranges[i]), &ranges[i]}};
    nvtxEventSubmit(domain, payload, 1); */
}

/* Submit the ranges at once in a batch. */
nvtxEventBatch_t batch = {0};
batch.eventSchemaId = schemaId;
batch.events = ranges;
batch.size = sizeof(ranges);

nvtxEventBatchSubmit(domain, &batch);
```

For a mark, use one `NVTX_PAYLOAD_ENTRY_FLAG_MARK | NVTX_PAYLOAD_ENTRY_FLAG_TIMESTAMP`
entry instead of the begin/end pair, and set `attr.flags = NVTX_PAYLOAD_SCHEMA_FLAG_MARK`.

Deferred events can specify their execution scope per event, per batch, or via scope
semantics; see [Scopes](#NVTX_EXTENDED_PAYLOADS_SCOPES) for the available forms and
precedence.

## Helper Macros {#NVTX_EXTENDED_PAYLOADS_HELPERS}

NVTX provides two kinds of helper macros for extended payloads:

- Schema-definition helpers that define a C struct and matching payload schema.
- Event-attribute helpers that attach payload data to a range or marker through
  @ref nvtxEventAttributes_v2 "nvtxEventAttributes_t".

For simpler schema authoring, `nvtx3/nvToolsExtPayloadHelper.h` provides macros that
derive a schema directly from a C struct definition:

- @c NVTX_DEFINE_STRUCT_WITH_SCHEMA defines the struct and the matching schema
  side-by-side.
- @c NVTX_DEFINE_SCHEMA_FOR_STRUCT generates a schema for an existing
  struct.
- @c NVTX_DEFINE_STRUCT_WITH_SCHEMA_AND_REGISTER and
  @c NVTX_DEFINE_SCHEMA_FOR_STRUCT_AND_REGISTER additionally register the schema,
  exposing it as `uint64_t <name>_schemaId`.
- @c NVTX_PAYLOAD_SCHEMA_REGISTER registers a schema generated by the
  macros above.

Example:

```c
#include <nvtx3/nvToolsExtPayload.h>
#include <nvtx3/nvToolsExtPayloadHelper.h>

NVTX_DEFINE_STRUCT_WITH_SCHEMA_AND_REGISTER(domain, Step, "training.step",
    NVTX_PAYLOAD_ENTRIES(
        (uint64_t, iteration, TYPE_UINT64, "Training step index"),
        (float,    loss,      TYPE_FLOAT,  "Training loss")
    )
)

Step value = {42, 0.125f};
nvtxPayloadData_t pld = {Step_schemaId, sizeof(value), &value};
nvtxMarkPayload(domain, &pld, 1);
```

On MSVC, these macros require the conforming preprocessor (`/Zc:preprocessor` on
VS 2019+, `/experimental:preprocessor` on VS 2017 v15.5+). GCC, Clang, and other
conforming compilers work without extra flags.

The event-attribute helpers are lower-level convenience macros for attaching already
registered payload data to event attributes. Use @ref nvtxPayloadRangePush or
@ref nvtxPayloadMark for one payload, or @ref NVTX_PAYLOAD_EVTATTR_SET_MULTIPLE to bind
an array of `nvtxPayloadData_t` to @ref nvtxEventAttributes_v2 "nvtxEventAttributes_t".

## Best Practices {#NVTX_EXTENDED_PAYLOADS_BEST_PRACTICES}

- Register schemas, enums, scopes, and time domains **once** during initialization.
- Put stable metadata in registered objects or schema semantics when possible. For
  example, use registered strings for repeated names, registered enums for value names,
  and scope semantics for fixed scopes instead of adding repeated strings or scope
  entries to every event payload.
- Prefer explicit-width types (`NVTX_PAYLOAD_ENTRY_TYPE_UINT32`,
  `NVTX_PAYLOAD_ENTRY_TYPE_INT64`, `NVTX_PAYLOAD_ENTRY_TYPE_FLOAT32`, ...) over
  platform-dependent types (`INT`, `LONG`, `SIZE`) so tools on other platforms decode
  the blob correctly.
- Entry names are optional, but tools can use them as labels in UIs and exports, and
  may use unique entry names as identifiers within a schema. Provide names for anything
  a user or tool may need to reference.
- Applications should generally not make execution depend on NVTX API results,
  including whether instrumentation is enabled. Use @ref nvtxDomainIsEnabled only to
  avoid expensive instrumentation work, such as building a non-trivial payload.

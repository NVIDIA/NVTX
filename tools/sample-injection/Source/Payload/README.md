# Payload Parsing & Schema Finalization

This directory contains the tool-side implementation for decoding NVTX extended
payloads. It turns raw binary blobs and registered schema descriptors into typed,
formatted output.

## Architecture overview

```
                  ┌──────────────────────────┐
  NVTX API calls  │  NvtxPayloadInjection    │  Injection adapter (NVTX API
  ───────────────▶│  RegisterSchema/Enum()   │  validation + dynamic ID gen)
                  │  (../NvtxPayloadInjec-   │  Lives outside this directory.
                  │   tionAdapter.h/.cpp)    │
                  └────────────┬─────────────┘
                               │ delegates to
                               ▼
                  ┌──────────────────────────┐
                  │  NvtxPayloadRegistry     │  Owns schemas/enums/strings;
                  │  AddSchema/Enum()        │  central module entry point
                  │  DescribePayloads()      │
                  └────────────┬─────────────┘
                               │ delegates to
                               ▼
                  ┌──────────────────────────┐
                  │  NvtxPayloadParser       │  Binary decoding engine
                  │  ProcessPayloads()       │
                  └────────────┬─────────────┘
                               │ streams events to
                               ▼
                  ┌──────────────────────────┐
                  │  PayloadStreamVisitor    │  Abstract event sink
                  │  (interface)             │
                  └──────────────────────────┘
                          △       △
              ┌───────────┘       └─────────┐
              │          implement          │
 ┌─────────────────────────┐   ┌─────────────────────────┐
 │ NvtxPayloadTextVisitor  │   │ NvtxPayloadJsonVisitor  │
 │ (human-readable text)   │   │ (structured JSON)       │
 └─────────────────────────┘   └─────────────────────────┘
```

Consumers that already have pre-assigned schema IDs (e.g. trace-file readers)
can use `NvtxPayloadRegistry` directly, bypassing the injection adapter.

Note that in NVTX, static schema/enum IDs are only required to be unique per
domain. A single `NvtxPayloadRegistry` instance works when static IDs do not
collide across domains; if they could collide, use one registry per domain.

## File inventory


| File | Role |
|---|---|
| `NvtxPayloadRegistry.h/.cpp` | Owns registered schemas, enums, and string handles; provides decode/format operations. Primary module entry point for standalone consumers. |
| `NvtxPayloadParser.h/.cpp` | Core decoding engine: iterates payload fields according to schema layout and emits a depth-first event stream via `PayloadStreamVisitor`. |
| `NvtxPayloadSchema.h/.cpp` | Schema/enum data model (`PayloadSchema`, `PayloadSchemaEntry`, `PayloadEnum`) and eager finalization logic (`NvtxPayloadSchemaProcessor`). |
| `NvtxPayloadTextFormatter.h/.cpp` | `PayloadStreamVisitor` producing compact `name=value, ...` text. |
| `NvtxPayloadJsonFormatter.h/.cpp` | `PayloadStreamVisitor` producing JSON objects. |
| `NvtxPayloadUtils.h` | Shared helpers: alignment arithmetic, C-string type predicates, embedded-string detection, array-length extraction. |
| `UtfStringConversion.h` | UTF-16/32 to UTF-8 conversion helpers for wide C-strings in payloads. |


## Schema registration

`NvtxPayloadInjection::RegisterSchema` and `RegisterEnum` (in
`NvtxPayloadInjectionAdapter.h/.cpp`, next to `NvtxSampleInjection.cpp`) are called from the
injection callbacks (`impl::PayloadSchemaRegister` / `impl::PayloadEnumRegister`).
They validate the incoming `nvtxPayloadSchemaAttr_t` or `nvtxPayloadEnumAttr_t`, normalize the
entries into the internal `PayloadSchema` / `PayloadEnum` representations, assign a schema ID
(either user-provided or auto-generated from a dynamic counter), and delegate storage and
finalization to the underlying `NvtxPayloadRegistry`.

Code that already has pre-assigned IDs and pre-built schema/enum objects (for example a
trace-file reader) can call `NvtxPayloadRegistry::AddSchema` / `AddEnum` directly,
bypassing validation and ID generation.

**Schema ID ranges:**

| Range                                                                           | Purpose                                  |
| ------------------------------------------------------------------------------- | ---------------------------------------- |
| `0 .. NVTX_PAYLOAD_SCHEMA_ID_STATIC_START-1`                                    | Reserved for predefined NVTX entry types |
| `NVTX_PAYLOAD_SCHEMA_ID_STATIC_START .. NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START-1` | User-assigned static IDs                 |
| `≥ NVTX_PAYLOAD_SCHEMA_ID_DYNAMIC_START`                                        | Auto-assigned dynamic IDs                |

Static IDs are given by the user during registration. Dynamic IDs are
auto-incremented by the processor when no explicit ID is provided.

## Supported schema types

- **Static schemas** (`NVTX_PAYLOAD_SCHEMA_TYPE_STATIC`) &mdash; Fixed-size
  payloads analogous to C structs. Field offsets can be explicit or computed
  implicitly from type sizes and alignment (including `packAlign`).

- **Dynamic schemas** (`NVTX_PAYLOAD_SCHEMA_TYPE_DYNAMIC`) &mdash;
  Variable-length payloads where some fields have runtime-determined sizes
  (length-indexed arrays, zero-terminated strings). Offsets are advanced while
  parsing.

## Supported entry features

| Feature | Flag / mechanism |
|---|---|
| Predefined scalar types | `NVTX_PAYLOAD_ENTRY_TYPE_*` (integers, floats, bytes, address, color) |
| C strings (ASCII, UTF-8/16/32) | `NVTX_PAYLOAD_ENTRY_TYPE_CSTRING*` |
| Embedded fixed-size strings | Non-zero `arrayOrUnionDetail` without `FLAG_POINTER` |
| Registered string handles | `NVTX_PAYLOAD_ENTRY_TYPE_NVTX_REGISTERED_STRING_HANDLE` |
| Fixed-size arrays | `NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_FIXED_SIZE` |
| Length-indexed arrays | `NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX` |
| Zero-terminated arrays | `NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_ZERO_TERMINATED` |
| Nested schemas | Entry `type` set to a registered schema ID |
| Enums and flag enums | Entry `type` set to a registered enum ID |

## Schema finalization

Schemas are finalized **eagerly** at insertion time inside
`NvtxPayloadRegistry::AddSchema`. All nested schema dependencies must
already be registered and finalized before they are referenced. Cyclic
dependencies are not supported. If finalization fails (for example due to
unresolved dependencies or invalid layout), registration returns 0 and the
schema is not stored.

### What finalization computes

1. **Array layout classification**: Each entry's `flags` are translated
  into a `PayloadArrayLayout` enum (`None`, `FixedSize`, `LengthIndex`,
   `ZeroTerminated`).
2. **Element sizes**: For predefined types, sizes come from the
  `nvtxPayloadEntryTypeInfo_t` table (provided by the payload extension at init time)
   or from built-in defaults. For nested static schemas, the already-finalized
   schema's `staticPayloadSize` becomes the element size. Dynamic nested schemas
   may only appear as scalar entries (not in arrays, since there is no fixed
   stride); their actual consumed size is resolved at parse time.
3. **Implicit offset resolution**: Entries whose `offset` is 0 (except
  the first entry) get their offset computed by advancing a cursor with proper
   alignment. The effective alignment of each member is capped by the schema's
   `packAlign` value (analogous to `#pragma pack`).
4. **Schema alignment**: The maximum effective member alignment across
  all entries becomes the schema's `alignment` (used when the schema itself is
   nested inside another schema).
5. **Static payload size**: For static schemas, the total size must cover
  all finalized entries. If the user-specified size is too small or missing,
   finalization logs an error and auto-adjusts upward.
6. **Length-source marking**: Entries referenced by
  `NVTX_PAYLOAD_ENTRY_FLAG_ARRAY_LENGTH_INDEX` arrays have their
   `isArrayLength` flag set, so the parser knows to capture their decoded value
   during field emission.
7. **Entry kind resolution**: Custom type IDs (`≥ NVTX_PAYLOAD_SCHEMA_ID_STATIC_START`)
  are classified as `NestedSchema`, `Enum`, or `Unknown` based on what is
   currently registered.

## Payload parsing

`NvtxPayloadParser::ProcessPayloads` is the top-level decode entry point. For
each `nvtxPayloadData_t` element it:

1. **Validates** the payload pointer, size, and schema ID.
2. **Resolves `SIZE_MAX`** payloads for null-terminated C-string types by
  scanning for the terminator.
3. **Routes predefined types** (schema ID `< NVTX_PAYLOAD_SCHEMA_ID_STATIC_START`)
  to `EmitPredefinedTypePayload`. If the payload size is a multiple of the element size,
  it is auto-detected as a fixed-size array.
4. **Looks up** the registered (and already finalized) schema for custom IDs,
  then iterates fields via `VisitSchemaFields`.

### Field emission

`EmitField` dispatches each schema entry to a type-specific handler:


| Handler                     | Handles                                                                                                                |
| --------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| `EmitIntegerField`          | Signed/unsigned integers; also captures values for length-index arrays.                                                |
| `EmitFloatingPointField`    | float, double, long double (and raw bytes for exotic widths like float16).                                             |
| `EmitCStringField`          | All C-string variants: pointer-based, embedded fixed, length-indexed, zero-terminated, including UTF-16/32 conversion. |
| `EmitRegisteredStringField` | `nvtxStringHandle_t` fields resolved via the registered-string map.                                                    |
| `EmitNestedSchemaField`     | Recursively visits nested schema objects/arrays.                                                                       |
| `EmitEnumField`             | Resolves enum values by exact match or flag-combination.                                                               |


For each field, a `FieldDecodeInfo` is computed first via `BuildFieldDecodeInfo`.
This struct holds the resolved absolute byte offset, element size, array length,
and whether a zero-terminator was found. For dynamic schemas, implicit offsets
are computed on the fly using cursor advancement with alignment.

### Streaming visitor model

The parser does not build an intermediate data structure. Instead it emits a
depth-first stream of typed events through the `PayloadStreamVisitor` interface:

```
OnBeginPayloads(count)
  OnPayloadBegin(index, schemaId, size, name)
    OnFieldBegin(name, description)
      [OnArrayBegin(length)]
        OnSignedInteger / OnUnsignedInteger / OnFloatingPoint /
        OnString / OnRawBytes /
        OnObjectBegin ... OnObjectEnd   (nested schemas)
      [OnArrayEnd]
    OnFieldEnd
    ...
  OnPayloadEnd
  ...
OnEndPayloads
```

This design keeps memory usage constant regardless of payload complexity and
allows formatters to produce output incrementally.

## Output formatters

Both formatters implement `PayloadStreamVisitor` and accumulate output into an
internal `std::ostringstream`, returned by `Finish()`.

### Text format (`NvtxPayloadTextVisitor`)

Produces compact, flat output designed for log lines:

```
counter=1, measurement=1.5
```

Arrays are rendered as `[v1, v2, ...]`, nested objects as `{field=val, ...}`.

### JSON format (`NvtxPayloadJsonVisitor`)

Produces valid JSON objects:

```json
{"counter": 1, "measurement": 1.5}
```

The JSON formatter escapes string values, generates unique keys for unnamed
fields, and emits `null` for fields that could not be decoded.

Select the format at runtime by setting the `NVTX_PAYLOAD_FORMAT` environment
variable to `json` before the injection is loaded. The default is `text`.

## Known limitations

### Unsupported entry flags

| Flag | Effect |
|---|---|
| `ARRAY_LENGTH_PAYLOAD_INDEX` | Array length stored in a separate payload of the same event. Entries using this flag are treated as scalars. |
| `POINTER` (non-string types) | Only honoured for C-string entries. For integers, floats, etc. the stored value is read inline; no pointer indirection is performed. |
| `OFFSET_FROM_BASE` / `OFFSET_FROM_HERE` | Indirection flags that reinterpret the entry value as an offset or pointer to the actual data. Entries using these flags are parsed as if the value were inline. |
| `DEEP_COPY` | Irrelevant for read-only decode; the parser never copies payload buffers. |
| `HIDE` | The parser does not filter hidden entries from the output. |

### Unsupported semantic entry flags

The following flags are preserved in the schema entry but do not influence
decode behaviour or output formatting: `EVENT_MESSAGE`, `TIMESTAMP`,
`RANGE_BEGIN`, `RANGE_END`, `MARK`, `COUNTER`.

### Unsupported schema-level flags

Schema-level flags (`nvtxPayloadSchemaAttr_t::flags`) are not stored or acted
upon. This includes `DEEP_COPY`, `REFERENCED`, `COUNTER_GROUP`,
`RANGE_PUSHPOP`, `RANGE_STARTEND`, `RANGE_PUSH`, `RANGE_POP`, `RANGE_START`,
`RANGE_END`, and `MARK`.

### Other

- Union schema types (`NVTX_PAYLOAD_SCHEMA_TYPE_UNION` and
  `UNION_WITH_INTERNAL_SELECTOR`) are rejected at finalization.
- `NVTX_TYPE_PAYLOAD_SCHEMA_REFERENCED` payloads are silently skipped.
- `NVTX_TYPE_PAYLOAD_SCHEMA_RAW` payloads are emitted as raw hex bytes.
- Deferred-event APIs (`nvtxTimestampGet`, `nvtxTimeDomainRegister`,
  `nvtxEventSubmit`, `nvtxEventBatchSubmit`, etc.) and `nvtxRangeStartPayload`,
  `nvtxRangeEndPayload`, `nvtxDomainIsEnabled`, `nvtxScopeRegister` are not
  wired up in the sample injection.

// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#![cfg(all(
    feature = "alloc",
    feature = "payload",
    feature = "counters",
    feature = "memory"
))]

use nvtx::{
    payload_schema, BatchOrder, CounterBatchOptions, CounterError, CounterSemantic, Domain,
    EventBatch, MemoryError, MemoryHeapDescriptor, MemoryHeapUsage, NoValueReason, PayloadData,
    PayloadEntryFlag, PayloadEntryType, PayloadError, PayloadSchemaEntry, PermissionAccess,
    PermissionAssignment, PermissionBindFlags, PermissionBindScope, PermissionCreationFlags,
    ScopeId, SemanticError, TimeDomainBuilder, TimeDomainId, TimeSemantic, TimestampType,
    VirtualRange,
};
use std::error::Error;

#[repr(C)]
struct Sample {
    count: u32,
    value: f64,
}

payload_schema!(
    Sample,
    c"Sample",
    {
        count: PayloadEntryType::UINT32,
        value: PayloadEntryType::FLOAT64,
    }
);

#[test]
fn payload_flag_builder_validates_exclusive_groups() {
    let valid = PayloadSchemaEntry::builder(PayloadEntryType::CSTRING_UTF8)
        .flags(PayloadEntryFlag::event().array_zero_terminated())
        .build();
    assert!(valid.is_ok());

    let invalid = PayloadSchemaEntry::builder(PayloadEntryType::UINT32)
        .flags(
            PayloadEntryFlag::new()
                .array_fixed_size()
                .array_zero_terminated(),
        )
        .build();
    assert!(matches!(invalid, Err(PayloadError::ConflictingArrayModes)));
}

#[test]
fn schema_macro_and_payload_calls_are_usable() -> Result<(), Box<dyn Error>> {
    let domain = Domain::new(c"extensions-test");
    let schema = domain.payload_schema::<Sample>()?;
    let value = Sample {
        count: 7,
        value: 3.5,
    };
    let payload = PayloadData::from_value(schema, &value);

    domain.mark_payloads(&[payload]);
    domain.submit_event(&[payload]);
    let range = domain.range_payloads(&[payload]);
    range.finish_with_payloads(&[]);
    let local = domain.local_range_payloads(&[payload]);
    let _ = local.pop_with_payloads(&[]);

    let attributes = domain.event_attributes_builder().message(c"sample").build();
    domain.mark_with_payloads(&attributes, &[payload])?;
    let counter = domain
        .payload_counter::<Sample>(c"sample-counter")?
        .register();
    counter.sample(&value);
    Ok(())
}

#[test]
fn semantic_builders_reject_invalid_values() {
    assert!(matches!(
        CounterSemantic::new().unit_scale(0, 1),
        Err(SemanticError::ZeroUnitScale)
    ));
    assert!(matches!(
        CounterSemantic::new().limits(5_i64, 1_i64),
        Err(SemanticError::InvalidLimits)
    ));
    assert!(matches!(
        CounterSemantic::new().limits(1_i64, 2_u64),
        Err(SemanticError::LimitTypeMismatch)
    ));
}

#[test]
fn scope_time_and_deferred_batch_validation_work() -> Result<(), Box<dyn Error>> {
    let domain = Domain::new(c"scope-time-test");
    let scope = domain.register_scope(c"worker", ScopeId::ROOT, None)?;
    let time_domain = domain.register_time_domain(
        TimeDomainBuilder::new(TimestampType::TOOL_PROVIDED).scope(scope.id()),
    );
    time_domain.sync_point(TimeDomainId::predefined(TimestampType::CPU_TSC), 1, 2);
    let _ = nvtx::timestamp();

    let events = [0_u8; 16];
    assert!(matches!(
        EventBatch::new(nvtx::SchemaId::from_raw(1), &events).flex_data(&events, 17),
        Err(PayloadError::InvalidFlexDataOffset)
    ));

    let semantics = nvtx::SemanticChain::new().semantic(TimeSemantic::new(time_domain.id()));
    let entry = PayloadSchemaEntry::builder(PayloadEntryType::INT64)
        .semantics(semantics)
        .build();
    assert!(entry.is_ok());
    Ok(())
}

#[test]
fn typed_counters_validate_and_submit_batches() -> Result<(), Box<dyn Error>> {
    let domain = Domain::new(c"counter-test");
    let counter = domain
        .counter::<i64>(c"iterations")?
        .semantics(CounterSemantic::new().unit(c"items"))
        .register();
    counter.sample(&7);
    counter.sample_no_value(NoValueReason::Unchanged);

    let mismatch = counter.submit_batch(
        &[1, 2],
        Some(&[10]),
        CounterBatchOptions::new().order(BatchOrder::TimeSorted),
    );
    assert!(matches!(
        mismatch,
        Err(CounterError::TimestampCountMismatch)
    ));
    counter.submit_batch(&[1, 2], Some(&[10, 20]), CounterBatchOptions::new())?;
    Ok(())
}

#[test]
fn memory_raii_and_permission_validation_are_usable() -> Result<(), Box<dyn Error>> {
    let domain = Domain::new(c"memory-test");
    let bytes = [0_u8; 64];
    let range = VirtualRange::from_slice(&bytes);
    let descriptor =
        MemoryHeapDescriptor::virtual_address(MemoryHeapUsage::Layout, range).name(c"heap");
    // SAFETY: `bytes` outlives the heap and every region registered below.
    let heap = unsafe { domain.register_memory_heap(&descriptor) };
    let regions = heap.register_virtual_regions(&[range])?;
    regions.name_all(&[c"buffer"], 0)?;
    // SAFETY: The resized range still borrows `bytes`, which outlives `regions`.
    unsafe { regions.resize_virtual_regions(&[range])? };

    let region = regions.region(0).ok_or(MemoryError::RegionOutOfBounds)?;
    let permissions =
        domain.create_memory_permissions(PermissionCreationFlags::new().exclude_global_write());
    permissions.assign(&[PermissionAssignment::new(
        region,
        PermissionAccess::none().read().write(),
    )])?;
    let binding = permissions.bind(
        PermissionBindScope::CpuThread,
        PermissionBindFlags::new().strict_write(),
    );
    drop(binding);

    assert!(matches!(
        permissions.assign(&[PermissionAssignment::new(
            region,
            PermissionAccess::none().reset().read(),
        )]),
        Err(MemoryError::ResetWithAccess)
    ));
    Ok(())
}

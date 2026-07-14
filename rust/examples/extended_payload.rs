// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use nvtx::{payload_schema, Domain, PayloadData, PayloadEntryType, PayloadError};

#[repr(C)]
struct SensorReading {
    sensor_id: u32,
    temperature: f64,
}

payload_schema!(
    SensorReading,
    c"SensorReading",
    {
        sensor_id: PayloadEntryType::UINT32,
        temperature: PayloadEntryType::FLOAT64,
    }
);

fn main() -> Result<(), PayloadError> {
    let domain = Domain::new(c"telemetry");
    let schema = domain.payload_schema::<SensorReading>()?;
    let reading = SensorReading {
        sensor_id: 7,
        temperature: 21.5,
    };
    domain.mark_payloads(&[PayloadData::from_value(schema, &reading)]);
    Ok(())
}

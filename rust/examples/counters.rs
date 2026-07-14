// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use nvtx::{CounterBatchOptions, CounterError, CounterSemantic, Domain, ScopeId};

fn main() -> Result<(), CounterError> {
    let domain = Domain::new(c"training");
    let loss = domain
        .counter::<f64>(c"loss")?
        .scope(ScopeId::CURRENT_SW_PROCESS)
        .semantics(CounterSemantic::new().unit(c"ratio"))
        .register();

    loss.sample(&0.42);
    loss.submit_batch(
        &[0.42, 0.31, 0.25],
        Some(&[100, 200, 300]),
        CounterBatchOptions::new(),
    )?;
    Ok(())
}

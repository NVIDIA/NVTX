// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use std::{thread, time};

fn s(value: &str) -> nvtx::Str {
    nvtx::Str::from_str_lossy(value)
}

fn main() {
    // we must hold ranges with a proper name
    // _ will not work since drop() is called immediately
    let _x = nvtx::LocalRange::new(s("Start 🦀"));
    thread::sleep(time::Duration::from_millis(5));
    for i in 1..=10 {
        {
            let _rng = nvtx::LocalRange::new(
                nvtx::EventAttributes::builder()
                    .color(nvtx::color::cornflowerblue)
                    .message(nvtx::Str::from_string_lossy(format!(
                        "Iteration Number {i}"
                    )))
                    .payload(i)
                    .build(),
            );
            for j in 1..=i {
                {
                    let _r = nvtx::LocalRange::new(
                        nvtx::EventAttributes::builder()
                            .color(nvtx::color::beige)
                            .payload(j)
                            .message(s("Inner"))
                            .build(),
                    );
                    thread::sleep(time::Duration::from_millis(j * 5));
                }
                thread::sleep(time::Duration::from_millis(5));
            }
        }
        thread::sleep(time::Duration::from_millis(10));
    }
}

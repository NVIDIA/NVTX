// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use std::{thread, time};

fn s(value: &str) -> nvtx::Str {
    nvtx::Str::from_str_lossy(value)
}

fn main() {
    // we must hold ranges with a proper name
    // _ will not work since drop() is called immediately
    let mut app = Some(nvtx::Range::new(
        nvtx::EventAttributes::builder()
            .color(nvtx::color::salmon)
            .message(s("Start 🦀"))
            .build(),
    ));
    thread::sleep(time::Duration::from_millis(5));
    for i in 10..=20 {
        {
            let mut iter = Some(nvtx::Range::new(
                nvtx::EventAttributes::builder()
                    .color(nvtx::color::cornflowerblue)
                    .message(nvtx::Str::from_string_lossy(format!(
                        "Iteration Number {i}"
                    )))
                    .payload(i)
                    .build(),
            ));
            for j in 1..=i {
                {
                    let inner = nvtx::Range::new(
                        nvtx::EventAttributes::builder()
                            .color(nvtx::color::beige)
                            .payload(j)
                            .message(s("Inner"))
                            .build(),
                    );
                    thread::sleep(time::Duration::from_millis(10));
                    drop(inner);
                    if j == i / 2 {
                        if let Some(range) = iter.take() {
                            drop(range);
                        }
                    }
                }
                thread::sleep(time::Duration::from_millis(5));
            }
        }

        thread::sleep(time::Duration::from_millis(10));
        if i == 15 {
            if let Some(range) = app.take() {
                drop(range);
            }
        }
    }
}

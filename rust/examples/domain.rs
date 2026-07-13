// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use std::{thread, time};

fn s(value: &str) -> nvtx::Str {
    nvtx::Str::from_str_lossy(value)
}

fn main() {
    let domain = nvtx::Domain::new(s("Domain"));
    let [alpha, beta, gamma] = domain.register_strings([s("alpha"), s("beta"), s("gamma")]);
    let a = domain.register_category(s("A"));

    let _r = domain.range(s("Duration"));

    let r1 = domain.range(
        domain
            .event_attributes_builder()
            .category(a)
            .color(nvtx::color::olive)
            .message(alpha)
            .build(),
    );

    let x = domain.user_sync(s("cool"));
    thread::sleep(time::Duration::from_millis(10));
    let y = x.acquire();
    thread::sleep(time::Duration::from_millis(10));
    let z = y.failed();
    thread::sleep(time::Duration::from_millis(10));
    let xx = z.acquire();
    thread::sleep(time::Duration::from_millis(10));
    let yy = xx.success();
    thread::sleep(time::Duration::from_millis(10));
    let zz = yy.release();
    drop(zz);

    thread::sleep(time::Duration::from_millis(10));
    let r2 = domain.range(
        domain
            .event_attributes_builder()
            .category(a)
            .color(nvtx::color::olive)
            .message(beta)
            .build(),
    );
    thread::sleep(time::Duration::from_millis(10));
    let r3 = domain.range(
        domain
            .event_attributes_builder()
            .category(a)
            .color(nvtx::color::olive)
            .message(gamma)
            .build(),
    );
    thread::sleep(time::Duration::from_millis(10));
    drop(r1);
    thread::sleep(time::Duration::from_millis(10));
    drop(r2);
    thread::sleep(time::Duration::from_millis(10));
    drop(r3);
    let d2 = nvtx::Domain::new(s("cool"));

    let b2 = d2.register_category(s("B"));
    let p1 = d2.range(
        d2.event_attributes_builder()
            .category(b2)
            .color(nvtx::color::orangered)
            .message(s("Alpha2"))
            .build(),
    );
    thread::sleep(time::Duration::from_millis(10));
    let p2 = d2.range(
        d2.event_attributes_builder()
            .category(b2)
            .color(nvtx::color::orangered)
            .message(s("Beta2"))
            .build(),
    );
    thread::sleep(time::Duration::from_millis(10));
    let p3 = d2.range(
        d2.event_attributes_builder()
            .category(b2)
            .color(nvtx::color::orangered)
            .message(s("Gamma2"))
            .build(),
    );
    thread::sleep(time::Duration::from_millis(10));
    drop(p3);
    thread::sleep(time::Duration::from_millis(10));
    drop(p2);
    thread::sleep(time::Duration::from_millis(10));
    drop(p1);
}

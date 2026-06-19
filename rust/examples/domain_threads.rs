// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

use std::{
    any::Any,
    thread::{self, sleep},
    time::Duration,
};

fn unwrap_join<T>(name: &str, result: thread::Result<T>) -> T {
    match result {
        Ok(value) => value,
        Err(payload) => {
            log_thread_panic(name, payload.as_ref());
            std::panic::resume_unwind(payload);
        }
    }
}

fn log_thread_panic(name: &str, payload: &(dyn Any + Send)) {
    if let Some(message) = payload.downcast_ref::<&str>() {
        eprintln!("thread {name} panicked: {message}");
    } else if let Some(message) = payload.downcast_ref::<String>() {
        eprintln!("thread {name} panicked: {message}");
    } else {
        eprintln!("thread {name} panicked with non-string payload");
    }
}

fn s(value: &str) -> nvtx::Str {
    nvtx::Str::from_str_lossy(value)
}

fn main() {
    let domain = nvtx::Domain::new(s("Domain"));
    let d = &domain;
    nvtx::name_current_thread(s("Main Thread"));

    thread::scope(|s| {
        let r = d.range(self::s("Start on main thread"));
        let t1 = s.spawn(move || {
            nvtx::name_current_thread(self::s("Fork 1"));
            sleep(Duration::from_millis(10));
            drop(r);
        });
        let t2 = s.spawn(move || {
            nvtx::name_current_thread(self::s("Fork 2"));
            let r = d.range(self::s("Start on Fork 2"));
            sleep(Duration::from_millis(20));
            r
        });
        let t3 = s.spawn(move || {
            nvtx::name_current_thread(self::s("Fork 3"));
            let _r = d.range(self::s("Start and end on Fork 3"));
            sleep(Duration::from_millis(30));
        });

        unwrap_join("t2", t2.join());
        unwrap_join("t1", t1.join());
        unwrap_join("t3", t3.join());
    });
}

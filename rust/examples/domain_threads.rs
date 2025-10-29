use std::{
    thread::{self, sleep},
    time::Duration,
};

fn main() {
    let domain = nvtx::Domain::new("Domain");
    let d = &domain;
    nvtx::name_current_thread("Main Thread");

    thread::scope(|s| {
        let r = d.range("Start on main thread");
        let t1 = s.spawn(move || {
            nvtx::name_current_thread("Fork 1");
            sleep(Duration::from_millis(10));
            drop(r)
        });
        let t2 = s.spawn(move || {
            nvtx::name_current_thread("Fork 2");
            let r = d.range("Start on Fork 2");
            sleep(Duration::from_millis(20));
            r
        });
        let t3 = s.spawn(move || {
            nvtx::name_current_thread("Fork 3");
            let _r = d.range("Start and end on Fork 3");
            sleep(Duration::from_millis(30))
        });

        t2.join().expect("Failed to join");
        t1.join().expect("Failed to join");
        t3.join().expect("Failed to join");
    });
}

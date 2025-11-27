use std::{
    thread::{self, sleep},
    time::Duration,
};

fn main() {
    nvtx::name_current_thread("Main Thread");
    let r = nvtx::Range::new("Start on main thread");
    let t1 = thread::spawn(move || {
        nvtx::name_current_thread("Fork 1");
        sleep(Duration::from_millis(10));
        drop(r)
    });
    let t2 = thread::spawn(move || {
        nvtx::name_current_thread("Fork 2");
        let r = nvtx::Range::new("Start on Fork 2");
        sleep(Duration::from_millis(20));
        r
    });
    let t3 = thread::spawn(move || {
        nvtx::name_current_thread("Fork 3");
        let _r = nvtx::Range::new("Start and end on Fork 3");
        sleep(Duration::from_millis(30))
    });

    t2.join().expect("Failed to join");
    t1.join().expect("Failed to join");
    t3.join().expect("Failed to join");
}

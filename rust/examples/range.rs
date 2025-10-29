use std::{thread, time};

fn main() {
    // we must hold ranges with a proper name
    // _ will not work since drop() is called immediately
    let _x = nvtx::LocalRange::new("Start 🦀");
    thread::sleep(time::Duration::from_millis(5));
    for i in 1..=10 {
        {
            let _rng = nvtx::LocalRange::new(
                nvtx::EventAttributes::builder()
                    .color(nvtx::color::cornflowerblue)
                    .message(format!("Iteration Number {i}"))
                    .payload(i)
                    .build(),
            );
            for j in 1..=i {
                {
                    let _r = nvtx::LocalRange::new(
                        nvtx::EventAttributes::builder()
                            .color(nvtx::color::beige)
                            .payload(j)
                            .message("Inner")
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

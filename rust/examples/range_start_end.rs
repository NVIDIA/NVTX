use std::{thread, time};

fn main() {
    // we must hold ranges with a proper name
    // _ will not work since drop() is called immediately
    let mut app = Some(nvtx::Range::new(
        nvtx::EventAttributes::builder()
            .color(nvtx::color::salmon)
            .message("Start 🦀")
            .build(),
    ));
    thread::sleep(time::Duration::from_millis(5));
    for i in 10..=20 {
        {
            let mut iter = Some(nvtx::Range::new(
                nvtx::EventAttributes::builder()
                    .color(nvtx::color::cornflowerblue)
                    .message(format!("Iteration Number {i}"))
                    .payload(i)
                    .build(),
            ));
            for j in 1..=i {
                {
                    let inner = nvtx::Range::new(
                        nvtx::EventAttributes::builder()
                            .color(nvtx::color::beige)
                            .payload(j)
                            .message("Inner")
                            .build(),
                    );
                    thread::sleep(time::Duration::from_millis(10));
                    drop(inner);
                    if j == i / 2 {
                        drop(iter.unwrap());
                        iter = None;
                    }
                }
                thread::sleep(time::Duration::from_millis(5));
            }
        }

        thread::sleep(time::Duration::from_millis(10));
        if i == 15 {
            drop(app.unwrap());
            app = None;
        }
    }
}

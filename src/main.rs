use std::{error::Error, thread, time::Duration};

fn main() -> ! {
    loop {
        println!("Hello, world!");
        thread::sleep(Duration::from_millis(10));
    }
}

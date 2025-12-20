use std::{thread, time::Duration};

fn main() -> ! {
    loop {
        println!("Hello World!");
        thread::sleep(Duration::from_millis(50));
    }
}

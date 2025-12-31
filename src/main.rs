#[cfg(feature = "spi")]
mod spi_interface;

#[cfg(feature = "i2c")]
mod i2c_interface;

mod config;

use tokio::runtime;
use tracing_subscriber::prelude::*;
use tracing_subscriber::util::SubscriberInitExt;
use tracing_subscriber::{EnvFilter, fmt};

fn main() {
    let rt = runtime::Builder::new_multi_thread()
        .enable_all()
        .thread_stack_size(1024 * 1024)
        .build()
        .unwrap_or_else(|err| panic!("Failed to create tokio lifetime! Error: {err}"));

    
    let filter = EnvFilter::from_default_env();

    #[cfg(feature = "tokio-console")]
    // Ensure that the console can see the tasks
    let filter = filter
        .add_directive("tokio::task=trace".parse().expect("Invalid filter directive!"))
        .add_directive("runtime=trace".parse().expect("Invalid filter directive!"));

    let subscriber = tracing_subscriber::registry()
        .with(fmt::layer())
        .with(filter);

    #[cfg(not(feature = "tokio-console"))]
    subscriber.init();

    #[cfg(feature = "tokio-console")]
    subscriber.with(console_subscriber::spawn()).init();


    rt.block_on(async move {
        #[cfg(feature = "spi")]
        tokio::spawn(spi_interface::interface_handler());
        #[cfg(feature = "i2c")]
        tokio::spawn(i2c_interface::interface_handler());

        // Keep the runtime alive.
        std::future::pending::<()>().await;
    });
}
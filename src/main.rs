#[cfg(feature = "spi")]
mod spi_interface;

#[cfg(feature = "i2c")]
mod i2c_interface;

mod config;

use tokio::runtime;
use tracing_subscriber::prelude::*;
use tracing_subscriber::util::SubscriberInitExt;
use tracing_subscriber::{EnvFilter, filter::LevelFilter, fmt};

fn main() {
    let rt = runtime::Builder::new_multi_thread()
        .enable_all()
        .thread_stack_size(1024 * 1024)
        .build()
        .unwrap_or_else(|err| panic!("Failed to create tokio lifetime! Error: {err}"));

    let subscriber = tracing_subscriber::registry()
        .with(fmt::layer())
        .with(LevelFilter::INFO)
        .with(EnvFilter::from_default_env());

    #[cfg(feature = "tokio-console")]
    if std::env::var("TOKIO_CONSOLE").is_ok() {
        subscriber.with(console_subscriber::spawn()).init();
    } else {
        subscriber.init();
    }

    #[cfg(not(feature = "tokio-console"))]
    subscriber.init();

    rt.block_on(async move {
        #[cfg(feature = "spi")]
        tokio::spawn(spi_interface::interface_handler());
        #[cfg(feature = "i2c")]
        tokio::spawn(i2c_interface::interface_handler());

        // Keep the runtime alive.
        std::future::pending::<()>().await;
    });
}

mod spi_interface;
mod config;

use std::error::Error;
use spi_interface::spi_interface_handler;
use tracing_subscriber::FmtSubscriber;

fn main() -> Result<(), Box<dyn Error>> {
    let subscriber = FmtSubscriber::builder()
        .with_max_level(tracing::Level::TRACE)
        .finish();

    tracing::subscriber::set_global_default(subscriber)?;


    let spi = config::spi::new()?;
    tokio::spawn(spi_interface_handler(spi));
    Ok(())
}
mod spi_interface;
mod config;

use spi_interface::spi_interface_handler;
use tokio::runtime;
use tracing_subscriber::FmtSubscriber;

fn main() {
    let rt = runtime::Builder::new_multi_thread()
        .enable_all()
        .thread_stack_size(1024 * 1024)
        .build()
        .unwrap_or_else(|err| panic!("Failed to create tokio lifetime! Error: {err}"));

    let subscriber = FmtSubscriber::builder()
        .with_max_level(tracing::Level::TRACE)
        .finish();
    tracing::subscriber::set_global_default(subscriber).expect("Failed to start tracing subscriber!");

    let spi = config::spi::new()
        .unwrap_or_else(|err| panic!("Failed to initialize SPI peripheral! Error: {err}"));

    #[cfg(feature = "imu")]
    {
        use bno055::Bno055;
        use rpi_pal::hal::Delay;
        use tracing::info;

        let i2c = config::i2c::new()
            .unwrap_or_else(|err| panic!("Failed to initlize I2C peripheral! Error {err}"));

        let mut delay = Delay::new();
        let mut imu = Bno055::new(i2c);
        imu.init(&mut delay).unwrap_or_else(|err| panic!("Failed to initiate IMU peripheral! Error: {err}"));

        #[cfg(feature = "imu-force-recalib")]
        {
            use bno055::BNO055OperationMode;
            imu.set_mode(BNO055OperationMode::NDOF, &mut delay)
            .unwrap_or_else(|err| panic!("Failed to set IMU to NDOF mode! Error: {err}"));
            info!("Calibrating ...\nPlease perform steps described in Datasheet section 3.1.1");
            while !imu.is_fully_calibrated().expect("Calibration check error!") {}

            let calib = imu.calibration_profile(&mut delay)
                .unwrap_or_else(|err| panic!("Failed to fetch imu calibration profile! Error: {err}"));
            
            // TODO: write calibration to binary file on disk, and load if present.
        }
    }

    rt.block_on(async move {
        tokio::spawn(spi_interface_handler(spi));

        // Keep the runtime alive.
        std::future::pending::<()>().await;
    });
}
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
        use std::fs;

        use bno055::{Bno055, BNO055Calibration};
        use rpi_pal::hal::Delay;

        let i2c = config::i2c::new()
            .unwrap_or_else(|err| panic!("Failed to initlize I2C peripheral! Error {err}"));

        let mut delay = Delay::new();
        let mut imu = Bno055::new(i2c);
        imu.init(&mut delay).unwrap_or_else(|err| panic!("Failed to initiate IMU peripheral! Error: {err}"));

        #[cfg(not(feature = "imu-force-recalib"))]
        if fs::exists(config::imu::CALIB_CACHE_LOCATION)
            .unwrap_or_else(|err| panic!("Failed to get imu calibration cache file status. Error {err}"))
        {            
            let calib_profile = BNO055Calibration::from_buf(fs::read(config::imu::CALIB_CACHE_LOCATION)
                .unwrap_or_else(|err| panic!("Failed to read imu calibration cache data! Error: {err}"))
                .as_slice()
                .try_into()
                .expect("Invalid imu calibration cache size!"));
            
            imu.set_calibration_profile(calib_profile, &mut delay)
                .unwrap_or_else(|err| panic!("Failed to set imu calibration profile! Error: {err}"));
        } else {
            calibrate_imu(&mut imu, &mut delay);
            write_imu_calib(&mut imu, &mut delay);
        }

        #[cfg(feature = "imu-force-recalib")]
        {
            calibrate_imu(&mut imu, &mut delay);
            write_imu_calib(&mut imu, &mut delay);
        }

        // TODO: Other bno055 configuration; axis maps, etc.
    }

    rt.block_on(async move {
        tokio::spawn(spi_interface_handler(spi));

        // Keep the runtime alive.
        std::future::pending::<()>().await;
    });
}

#[cfg(feature = "imu")]
#[inline(always)]
fn calibrate_imu(imu: &mut bno055::Bno055<rpi_pal::i2c::I2c>, delay: &mut dyn embedded_hal::delay::DelayNs) {
    let mode_orig = imu.get_mode()
        .unwrap_or_else(|err| panic!("Failed to get IMU mode! Error: {err}"));

    imu.set_mode(bno055::BNO055OperationMode::NDOF, delay)
        .unwrap_or_else(|err| panic!("Failed to set IMU to NDOF mode! Error: {err}"));

    tracing::info!("Calibrating ...\nPlease perform steps described in Datasheet section 3.1.1");
    while !imu.is_fully_calibrated().expect("Calibration check error!") {}

    imu.set_mode(mode_orig, delay)
        .unwrap_or_else(|err| panic!("Failed to restore original IMU mode! Error: {err}"));
}

#[cfg(feature = "imu")]
#[inline(always)]
fn write_imu_calib(imu: &mut bno055::Bno055<rpi_pal::i2c::I2c>, delay: &mut dyn embedded_hal::delay::DelayNs) {
    let calib = imu.calibration_profile(delay)
        .unwrap_or_else(|err| panic!("Failed to fetch imu calibration profile! Error: {err}"));

    std::fs::write(config::imu::CALIB_CACHE_LOCATION, calib.as_bytes())
        .unwrap_or_else(|err| panic!("Failed to write imu calibration catche to disk! Error {err}"));
}
use crate::config;
use core::cell::RefCell;
#[cfg(feature = "aht20")]
use ahtx0::blocking::ahtx0;
#[cfg(feature = "aht20")]
use embedded_hal_bus::i2c::RefCellDevice;
use rpi_pal::hal;
use tracing::{info, instrument};

#[instrument]
pub async fn interface_handler() {
    let i2c = config::i2c::new()
        .unwrap_or_else(|err| panic!("Failed to initalize I2C peripheral! Error: {err}"));

    // Small overhead of refcell when not using more than one sensor, doesn't matter in normal operation; changing isn't worth additional code complexity.
    let i2c_refcell = RefCell::new(i2c);

    #[cfg(any(feature = "imu", feature = "aht20"))]
    let mut delay = hal::Delay::new();

    // Initilize this outside of the block so that its in scope for the rest of the function
    #[cfg(feature = "imu")]
    let mut imu = init_imu(&i2c_refcell, &mut delay);

    #[cfg(feature = "aht20")]
    let mut aht20 = ahtx0(RefCellDevice::new(&i2c_refcell));

    loop {
        #[cfg(feature = "imu")]
        {
            if let Ok(data) = imu.accel_data() { info!("IMU accel data: {:?}", data); }
            if let Ok(data) = imu.gyro_data() { info!("IMU gyro data: {:?}", data); }
            if let Ok(data) = imu.mag_data() { info!("IMU mag data: {:?}", data); }
            if let Ok(data) = imu.temperature() { info!("IMU Temp Data: {:?}", data); }
        }

        #[cfg(feature = "aht20")]
        {
            if let Ok(data) = aht20.measure(&mut delay) { 
                info!("AHT20 Data: humidity: {} temperature: {}", data.humidity.as_percent(), data.temperature.as_degrees_celsius());
            }
        }        
    }
}

#[cfg(feature = "imu")]
fn init_imu<'a>(
    i2c_refcell: &'a RefCell<rpi_pal::i2c::I2c>,
    delay: &mut hal::Delay
) -> bno055::Bno055<RefCellDevice<'a, rpi_pal::i2c::I2c>> {
    use std::fs;
    use bno055::{BNO055Calibration, BNO055OperationMode};

    let mut imu = bno055::Bno055::new(RefCellDevice::new(i2c_refcell));
    imu.init(delay).unwrap_or_else(|err| panic!("Failed to initiate IMU peripheral! Error: {err}"));

    #[cfg(not(feature = "imu-force-recalib"))]
    if fs::exists(config::imu::CALIB_CACHE_LOCATION)
        .unwrap_or_else(|err| panic!("Failed to get imu calibration cache file status. Error {err}"))
    {            
        let calib_profile = BNO055Calibration::from_buf(fs::read(config::imu::CALIB_CACHE_LOCATION)
            .unwrap_or_else(|err| panic!("Failed to read imu calibration cache data! Error: {err}"))
            .as_slice()
            .try_into()
            .expect("Invalid imu calibration cache size!"));
        
        imu.set_calibration_profile(calib_profile, delay)
            .unwrap_or_else(|err| panic!("Failed to set imu calibration profile! Error: {err}"));
    } else {
        calibrate_imu(&mut imu, delay);
        write_imu_calib(&mut imu, delay);
    }

    #[cfg(feature = "imu-force-recalib")]
    {
        calibrate_imu(&mut imu, delay);
        write_imu_calib(&mut imu, delay);
    }

    imu.set_axis_remap(config::imu::get_axis_map().expect("Invalid axis map config!"))
        .unwrap_or_else(|err| panic!("Failed to set axis remap! Error: {err}"));

    imu.set_mode(BNO055OperationMode::AMG, delay)
        .unwrap_or_else(|err| panic!("Failed to set imu mode! Error: {err}"));

    imu.set_external_crystal(true, delay)
        .unwrap_or_else(|err| panic!("Failed to set imu external crystal status! Error: {err}"));

    imu
}

#[cfg(feature = "imu")]
fn calibrate_imu(
    imu: &mut bno055::Bno055<embedded_hal_bus::i2c::RefCellDevice<'_, rpi_pal::i2c::I2c>>, 
    delay: &mut rpi_pal::hal::Delay
) {
    use bno055::BNO055OperationMode;
    use std::time::Duration;

    let mode_orig = imu.get_mode()
        .unwrap_or_else(|err| panic!("Failed to get IMU mode! Error: {err}"));

    imu.set_mode(BNO055OperationMode::NDOF, delay)
        .unwrap_or_else(|err| panic!("Failed to set IMU to NDOF mode! Error: {err}"));

    tracing::info!("Calibrating ...\nPlease perform steps described in Datasheet section 3.1.1");
    loop {
        let status = imu.get_calibration_status()
            .unwrap_or_else(|err| panic!("Failed to get IMU calibration status! Error: {err}"));

        info!("Current IMU calibration status: gyro: {}, accelerometer: {}, magnometer: {}", status.gyr, status.acc, status.mag);

        if status.gyr == 3 && status.acc == 3 && status.mag == 3 { break; }
        std::thread::sleep(Duration::from_millis(config::imu::CALIB_READY_CHECK_INTERVAL));
    }

    imu.set_mode(mode_orig, delay)
        .unwrap_or_else(|err| panic!("Failed to restore original IMU mode! Error: {err}"));
}

#[cfg(feature = "imu")]
fn write_imu_calib(
    imu: &mut bno055::Bno055<embedded_hal_bus::i2c::RefCellDevice<'_, rpi_pal::i2c::I2c>>, 
    delay: &mut rpi_pal::hal::Delay
) {
    let calib = imu.calibration_profile(delay)
        .unwrap_or_else(|err| panic!("Failed to fetch imu calibration profile! Error: {err}"));

    std::fs::write(config::imu::CALIB_CACHE_LOCATION, calib.as_bytes())
        .unwrap_or_else(|err| panic!("Failed to write imu calibration catche to disk! Error {err}"));
}
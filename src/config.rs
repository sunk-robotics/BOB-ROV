#[cfg(feature = "spi")]
pub mod spi {
    use rpi_pal::spi::{self, Spi};

    #[inline(always)]
    pub fn new() -> Result<Spi, spi::Error> {
        Spi::new(BUS, SLAVE_SELECT, CLOCK_SPEED, MODE)
    }

    pub const BUS: spi::Bus = spi::Bus::Spi0;
    pub const SLAVE_SELECT: spi::SlaveSelect = spi::SlaveSelect::Ss0;
    pub const MODE: spi::Mode = spi::Mode::Mode0;
    pub const CLOCK_SPEED: u32 = 12_500_000;
}

#[cfg(feature = "i2c")]
pub mod i2c {

    use rpi_pal::i2c::{self, I2c};

    #[inline(always)]
    pub fn new() -> Result<I2c, i2c::Error> {
        let i2c = I2c::with_bus(BUS)?;
        Ok(i2c)
    }

    pub const BUS: u8 = 1;
}

#[cfg(feature = "imu")]
pub mod imu {
    use bno055::{AxisRemap, BNO055AxisConfig};

    pub const CALIB_CACHE_LOCATION: &str = "./cache/imu_calib";
    pub const CALIB_READY_CHECK_INTERVAL: u64 = 250; // ms

    // If some swaps the named axis with the axis inside the option.
    pub const SWAP_X: Option<BNO055AxisConfig> = None;
    pub const SWAP_Y: Option<BNO055AxisConfig> = None;
    pub const SWAP_Z: Option<BNO055AxisConfig> = None;

    // TODO: Other bno055 configuration; axis maps, etc.
    pub fn get_axis_map() -> Result<AxisRemap, ()> {
        let mut remap = AxisRemap::builder();

        if let Some(x) = SWAP_X { remap = remap.swap_x_with(x) }
        if let Some(y) = SWAP_Y { remap = remap.swap_y_with(y) }
        if let Some(z) = SWAP_Z { remap = remap.swap_z_with(z) }

        remap.build()
    }
}
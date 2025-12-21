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

#[cfg(feature = "imu")]
pub mod i2c {
    use rpi_pal::i2c::{self, I2c};

    #[inline(always)]
    pub fn new() -> Result<I2c, i2c::Error> {
        I2c::with_bus(BUS)
    }

    pub const BUS: u8 = 1;
}
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
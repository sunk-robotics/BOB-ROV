use rpi_pal::spi::Spi;
use tracing::{error, info, instrument};
use rp2040_dshot::encoder::{Frame as DShotFrame, StandardDShotVariant};

#[instrument]
pub async fn spi_interface_handler(spi: Spi) {
    let mut telemetry_buffer = [0u8; 10];

    let write_buffer: [u8; 2];
    if let Some(frame) = DShotFrame::<StandardDShotVariant>::from_throttle(1028, true) {    
        write_buffer = frame.inner().to_le_bytes();
    } else {
        error!("Failed to construct DShot command frame! Throttle exceeded maximum value!");
        return;
    }

    let mut read_buffer = [0u8; 2];

    let mut telemetry_byte_idx = 0;

    loop {
        if telemetry_byte_idx == 10 {
            info!("Read the following telemetry data: {:?}", telemetry_buffer);
            telemetry_byte_idx = 0;
        }

        if let Err(transfer_err) = spi.transfer(&mut read_buffer, &write_buffer) {
            error!("Failed to tranfer over SPI! Error: {}", transfer_err);
            continue;
        }

        telemetry_buffer[telemetry_byte_idx..telemetry_byte_idx+2].copy_from_slice(&read_buffer);

        telemetry_byte_idx += 2;
    }
}
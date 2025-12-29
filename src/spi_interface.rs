use tracing::{error, info, instrument, warn};
use rp2040_dshot::encoder::{Frame as DShotFrame, StandardDShotVariant, TelemetryFrame};
use crate::config;

#[instrument]
pub async fn interface_handler() {
    let spi = config::spi::new()
    .unwrap_or_else(|err| panic!("Failed to initialize SPI peripheral! Error: {err}"));


    let mut telemetry_buffer = [0u8; 10];

    let command: [u8; 2];
    if let Some(frame) = DShotFrame::<StandardDShotVariant>::from_throttle(1028, true) {    
        command = frame.inner().to_le_bytes();
    } else {
        error!("Failed to construct DShot command frame! Throttle exceeded maximum value!");
        return;
    }

    let mut write_buffer: [u8; 1];
    let mut read_buffer = [0u8];

    let mut telemetry_byte_idx = 0;

    loop {
        if telemetry_byte_idx == 10 {

            // let computed_crc = TelemetryFrame::compute_crc(&telemetry_buffer[..9]);
            
            // if computed_crc == telemetry_buffer[9] {
            //     // info!("Successfully read the following telemetry data: {:?}", telemetry_buffer);
            // } else {
            //     warn!(
            //         "Telemetry CRC mismatch! Expected {:08b}, got {:08b}. Attempting shift by one! Invalid telmetry frame: {:?}",
            //         computed_crc, telemetry_buffer[9], telemetry_buffer
            //     );

            //     // Do one transfer without storing the read result, to offset the telemetry array before the next iteration.
            //     write_buffer = [command[telemetry_byte_idx & 1]];
            //     if let Err(transfer_err) = spi.transfer(&mut read_buffer, &write_buffer) {
            //         error!("Failed to tranfer over SPI! Error: {}", transfer_err);
            //         continue;
            //     }
            // }

            info!("Read: {:?}", telemetry_buffer);

            telemetry_byte_idx = 0;
        }

        write_buffer = [command[telemetry_byte_idx & 1]];
        // info!("Data to write {:?}", write_buffer);
        if let Err(transfer_err) = spi.transfer(&mut read_buffer, &write_buffer) {
            error!("Failed to tranfer over SPI! Error: {}", transfer_err);
            continue;
        }

        telemetry_buffer[telemetry_byte_idx] = read_buffer[0];

        telemetry_byte_idx += 1;
    }
}
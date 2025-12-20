# Setup

## Host Machine
1. Install rustup, instructions can be found [here](https://rustup.rs)
2. Run `rustup target add aarch64-unknown-linux-gnu` to enable cross-compilation
3. Install just by running `cargo install just`
4. Update the `pi_host` variable in the [justfile](./justfile) with the local address of the pi

## Target Pi 5
1. Install Rasberry Pi OS x64 Lite
   - You can use the Raspberry Pi Imager, currently hosted [here](https://www.raspberrypi.com/software/)
   - Or if you'd prefer, the ISOs are available [here](https://www.raspberrypi.com/software/operating-systems/)
   - Alternatively you connect the Pi to ethernet first, you can use the built-in installer by holding shift after booting without a valid image
2. During install, create a user called `sunk`, or change the defined name in the [justfile](./justfile)
3. Run `sudo raspi-config`
2. Enable SSH, SPI, and I2C under `3 Interface Options`
3. You can chagne any other settings to your preferences, with particular attention to `Localization Options`

# Building, Running, and Testing
Thanks to the justfile this is rather trivial

### Options
You have the following options for easily building and syncing with the pi:
- Run `just build` to build.
- Run `just sync` to build and sync to the pi.
- Run `just deploy-binary` to build and deploy the binary to pi. (THIS ONLY COPIES THE BINARY AND DOES NOT PERFORM A SYNC)
- Run `just run` to build, deploy, and run the binary on the pi. (THIS ONLY COPIES THE BINARY AND DOES NOT PERFORM A SYNC)
- Run `just run-cargo` to build, sync, and `cargo run` on the pi.
- Run `just test-cargo` to build, sync and `cargo test` on the pi.

### Note:
The default behavior is, as is done with cargo, to build the dev profile.\
Prepend `release` to build for release.\
**Example:** run `just run release` to build, sync, and run a release build.\
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
## Options:
You have the following options for easily building and syncing with the pi:
- Run `just build` to build.
- Run `just sync` to build and sync to the pi.
- Run `just deploy-binary` to build and deploy the binary to pi. (THIS ONLY COPIES THE BINARY AND DOES NOT PERFORM A SYNC)
- Run `just run` to build, deploy, and run the binary on the pi. (THIS ONLY COPIES THE BINARY AND DOES NOT PERFORM A SYNC)
- Run `just run-cargo` to build, sync, and `cargo run` on the pi.
- Run `just test-cargo` to build, sync and `cargo test` on the pi.
## Cargo arguments:
You can pass any arguments after the commands that would be valid for the corresponding cargo command.

`build`, `sync`, `deploy-binary`, and `run` will take in the arguments to `cargo build`, as that is run as part of their process\
This is because `run` and `deploy-binary` build the executable on the host machine, and than copy it over for faster build times.

`run-cargo` and `test-cargo` take in the arguments to `cargo run` or `cargo test` respectively.
## Environment Variables:
The `run`, `run-cargo`, and `test-cargo` commands support passing environment variables to the Pi execution environment.

**For `run` (binary execution):**
```bash
just run "RUST_LOG=debug TOKIO_CONSOLE=1" -r --features tokio-console
```

**For `run-cargo` and `test-cargo`:**
```bash
just run-cargo "RUST_LOG=trace TOKIO_CONSOLE=1" -r --features tokio-console imu-force-recalib
just test-cargo "RUST_LOG=debug" -- --test-threads=1
```

Environment variables should be passed as a single quoted string containing space-separated `KEY=value` pairs, placed before any cargo arguments. 
Multiple environment variables can be set in the same string.

**Available variables include:**
- `RUST_LOG` Allows for a myriad of options, (more information can be found [here](https://docs.rs/tracing-subscriber/0.3.22/tracing_subscriber/filter/struct.EnvFilter.html)) although you like mainly use this to set the log level. Options include `trace`, `info`, `debug`, `warn`, and `error`.
- `TOKIO_CONSOLE` Setting this to 1 enables the tokio console. The `tokio-console` feature must be enabled for this flag to do anything.
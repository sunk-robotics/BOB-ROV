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

`build`, `deploy-binary`, and `run` will take in the arguments to `cargo build`, as that is run as part of their process\
This is because `run` and `deploy-binary` build the executable on the host machine, and than copy it over for faster build times.

`run-cargo` and `test-cargo` take in the arguments to `cargo run` or `cargo test` respectively.
## Environment Variables:
### Build-time vs Runtime Environment Variables
- **Build-time variables** (like `RUSTFLAGS`) affect compilation and must be set when cargo compiles the code
- **Runtime variables** (like `RUST_LOG`) affect the running program and are set when executing the binary

### For `build`, `run` & `deploy-binary` (pre-compiled binary):
Since the binary is built on the host machine, build-time variables must be set locally before calling `just`. Run-time variables can be passed to `run` after:
```bash
# Syntax: BUILD_ENVs just build/deploy-binary [cargo-args]
RUSTFLAGS="--cfg tokio_unstable" just build -r --features tokio-console
RUSTFLAGS="--cfg tokio_unstable" just deploy-binary -r --features tokio-console

# Syntax: BUILD_ENVs just run "RUNTIME_ENVs" [cargo-args]
RUSTFLAGS="--cfg tokio_unstable" just run "RUST_LOG=trace" -r --features tokio-console
```

### For `run-cargo` and `test-cargo`:
These commands compile on the Pi, so both build-time and runtime variables need to be passed through SSH:
```bash
# Syntax: just run-cargo/test-cargo "BUILD_ENVs" "RUNTIME_ENVs" [cargo-args]
just run-cargo 'RUSTFLAGS="--cfg tokio_unstable"' "RUST_LOG=trace" -r --features tokio-console
just test-cargo 'RUSTFLAGS="--cfg tokio_unstable"' "RUST_LOG=trace" -r --features tokio-console
```

**Note**: You may need single quotes around build-time variables to preserve the inner double quotes.

### Common use cases:
**Logging:**
More complete information on the `RUST_LOG` env syntax can be found [here](https://docs.rs/tracing-subscriber/0.3.22/tracing_subscriber/filter/struct.EnvFilter.html),
but the most useful option will likely be the logging level, which can be adjusted, in descending order of verbosity with with `RUST_LOG=trace/debug/info/warn/error`
```bash
# With pre-compiled binary
just run "RUST_LOG=debug" -r

# With cargo run
just run-cargo "" "RUST_LOG=debug" -r
```

**Tokio Console (requires unstable flag + feature):**
Detailed information can be found [here](https://docs.rs/tokio-console/0.1.14/tokio_console/)\
Generally:
- Install with `cargo install --locked tokio-console`
- Run one of the below command to run with console output
- run `tokio-console http://{addr}:6669`
```bash
# With pre-compiled binary
RUSTFLAGS="--cfg tokio_unstable" just run "RUST_LOG=trace" -r --features tokio-console

# With cargo run
just run-cargo 'RUSTFLAGS="--cfg tokio_unstable"' "RUST_LOG=trace" -r --features tokio-console
```

**Multiple environment variables:**
```bash
# Runtime only
just run "RUST_LOG=debug RUST_BACKTRACE=1" -r

# Build-time and runtime
just run-cargo 'RUSTFLAGS="--cfg tokio_unstable"' "RUST_LOG=trace RUST_BACKTRACE=full" --features tokio-console
```

**No environment variables needed:**
```bash
# Use empty strings for env parameters
just run-cargo "" "" -r
# Or omit them entirely
just run-cargo -r
```
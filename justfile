pi_user := "sunk"
pi_host := "192.168.1.174"
pi_path := "~/"

# Build with any additional cargo arguments
build *args="":
    cargo build -j $(nproc) {{args}}

# Sync project files to Pi
_sync-generic:
    rsync -avzrh --include='Cargo.toml' --include='Cargo.lock' --include='src/***' --include='.cargo/***' --exclude='*' ./ {{pi_user}}@{{pi_host}}:{{pi_path}}/

# Build and sync to Pi
sync *args="": (build args) _sync-generic

# Deploy binary to Pi (looks in debug by default, pass --release to change)
_deploy-binary-generic *args="":
    rsync -avz target/aarch64-unknown-linux-gnu/{{ if args =~ "--release|-r" { "release" } else { "debug" } }}/bob-rov {{pi_user}}@{{pi_host}}:{{pi_path}}/bob-rov

# Build and deploy binary to Pi
deploy-binary *args="": (build args) (_deploy-binary-generic args)

# Run binary on Pi (generic helper)
_run-generic:
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && ./bob-rov"

# Build, deploy, and run the binary on Pi
run *args="": (deploy-binary args) _run-generic

# Run cargo run on Pi (generic helper)
_run-cargo-generic *args="":
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && cargo run -j $(nproc) {{args}}"

# Sync and run cargo run on Pi (will rebuild there)
run-cargo *args="": sync (_run-cargo-generic args)

# Run cargo test on Pi (generic helper)
_test-cargo-generic *args="":
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && cargo test -j $(nproc) {{args}}"

# Sync and run cargo test on Pi (will rebuild there)
test-cargo *args="": sync (_test-cargo-generic args)
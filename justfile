pi_user := "sunk"
pi_host := "192.168.1.71"
pi_path := "~/software"

# Build with specified profile (debug or release)
build profile="debug":
    cargo build -j $(nproc) {{ if profile == "release" { "--release" } else { "" } }} --target aarch64-unknown-linux-gnu

# Sync project files to Pi
_sync-generic profile="debug":
    rsync -avzrh --include 'Cargo.toml' --include 'src' --include '.cargo' --exclude './'./ {{pi_user}}@{{pi_host}}:{{pi_path}}/

# Build and sync to Pi
sync profile="debug": (build profile) (_sync-generic profile)

# Deploy binary to Pi
_deploy-binary-generic profile="debug":
    rsync -avz target/aarch64-unknown-linux-gnu/{{profile}}/bob-rov {{pi_user}}@{{pi_host}}:{{pi_path}}/bob-rov

# Build and deploy binary to Pi
deploy-binary profile="debug": (build profile) (_deploy-binary-generic profile)

# Run binary on Pi (generic helper)
_run-generic:
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && ./bob-rov"

# Build, deploy, and run the binary on Pi
run profile="debug": (deploy-binary profile) _run-generic

# Run cargo run on Pi (generic helper)
_run-cargo-generic profile="debug":
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && cargo run -j $(nproc) {{ if profile == "release" { "--release" } else { "" } }}"

# Sync and run cargo run on Pi (will rebuild there)
run-cargo profile="debug": (sync profile) (_run-cargo-generic profile)

# Run cargo test on Pi (generic helper)
_test-cargo-generic profile="debug":
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && cargo test -j $(nproc) {{ if profile == "release" { "--release" } else { "" } }}"

# Sync and run cargo test on Pi (will rebuild there)
test-cargo profile="debug": (sync profile) (_test-cargo-generic profile)
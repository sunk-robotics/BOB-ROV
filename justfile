pi_user := "sunk"
pi_host := "192.168.1.174"
pi_path := "~/"
# PI's cargo path
cargo_path := "~/.cargo/bin/cargo"

# Build with any additional cargo arguments
build *args="":
    cargo build -j $(nproc) {{args}}

# Build and sync to Pi
sync:
    rsync -avzrh --include='Cargo.toml' --include='Cargo.lock' --include='src/***' --include='.cargo/***' --exclude='*' ./ {{pi_user}}@{{pi_host}}:{{pi_path}}/

# Build and deploy binary to Pi
deploy-binary *args="": (build args)
    rsync -avz target/aarch64-unknown-linux-gnu/{{ if args =~ "--release|-r" { "release" } else { "debug" } }}/bob-rov {{pi_user}}@{{pi_host}}:{{pi_path}}/bob-rov

# Build, deploy, and run the binary on Pi - usage: just run-with-env "VAR=value" -r
run env="" *build_args="": (deploy-binary build_args)
    ssh {{pi_user}}@{{pi_host}} "pkill bob-rov || true && cd {{pi_path}} && {{env}} ./bob-rov"

# Sync and run cargo run on Pi
run-cargo build_env="" run_env="" *args="": sync 
    ssh {{pi_user}}@{{pi_host}} 'pkill bob-rov || true && cd {{pi_path}} && {{build_env}} {{run_env}} {{cargo_path}} run -j $(nproc) {{args}}'

# Sync and run cargo test on Pi
test-cargo build_env="" run_env="" *args="": sync
    ssh {{pi_user}}@{{pi_host}} 'pkill bob-rov || true && cd {{pi_path}} && {{build_env}} {{run_env}} {{cargo_path}} test -j $(nproc) {{args}}'

kill-process:
    ssh {{pi_user}}@{{pi_host}} "pkill bob-rov"

tokio-console:
    ssh {{pi_user}}@{{pi_host}} "tokio-console"

ssh:
    ssh {{pi_user}}@{{pi_host}}
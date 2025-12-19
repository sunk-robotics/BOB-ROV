pi_user := "sunk"
pi_host := "10.1.57.16"
pi_path := "~/software"

build:
    cargo build --release --target aarch64-unknown-linux-gnu


sync: build
    rsync -avzrh --exclude 'target' --include 'target/aarch64-unknown-linux-gnu/release/my-app' ./ {{pi_user}}@{{pi_host}}:{{pi_path}}/

deploy-binary: build
    scp target/aarch64-unknown-linux-gnu/release/bob-rov {{pi_user}}@{{pi_host}}:{{pi_path}}/bob-rov

# Run the pre-compiled binary on Pi
run: deploy-binary
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && ./bob-rov"

# Run cargo run on Pi (will rebuild there)
run-cargo: sync
    ssh {{pi_user}}@{{pi_host}} "cd {{pi_path}} && cargo run"
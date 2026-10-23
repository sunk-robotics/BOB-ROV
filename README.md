# Setup

run `./setup.sh` after first downloading the repo to pull dependencies and build everything correctly before deployment or development

[rosdoc2](https://github.com/ros-infrastructure/rosdoc2) is used for documentation generation. You can install it with `apt install -y python3-rosdoc2`

# Third Party Dependencies

The following are either third party or need to be built from source for various reasons

| Library          | Version                 | URL                                          |
| ---------------- | ----------------------- | -------------------------------------------- |
| cpp-httplib      | v0.63.0                 | https://github.com/yhirose/cpp-httplib       |
| libdatachannel   | v0.24                   | https://github.com/Rtificer/libdatachannel   |
| BNO055_SensorAPI | fork of v2.0.6          | https://github.com/Rtificer/BNO055_SensorAPI |
| libcamera        | arducam_v0.5.1+20250708 | https://github.com/ArduCAM/libcamera         |
| camera_ros       | v0.6.0                  | https://github.com/christianrauch/camera_ros |

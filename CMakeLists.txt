cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# FMO-S3 port: ESP32-S3 DevKitC-1 (xiaozhi breadboard kit). The Passport-only
# BLE Recovery bootloader hook is removed for this board.
project(FoloToy-AI-Passport)

# Include the reset request as a resource in USB and community installation.
# Reinstalling the same binary rewrites this seed; normal reboots leave it alone.
file(WRITE "${CMAKE_BINARY_DIR}/fmo_install.bin" "FMO_RESET_ON_INSTALL_V1\n")
esptool_py_flash_to_partition(flash "fmo_install" "${CMAKE_BINARY_DIR}/fmo_install.bin")

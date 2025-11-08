# WiFi RTP/UDP Receiver for nRF7002-DK
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_rtp_receiver LANGUAGES CXX)

# Source files
target_sources(app PRIVATE
    src/main.cpp
    src/wifi_manager.cpp
    src/rtp_receiver.cpp
)

# Include directories
target_include_directories(app PRIVATE
    src/
)

# Enable C++17
set_property(TARGET app PROPERTY CXX_STANDARD 17)

# WiFi RTP/UDP Receiver for nRF7002-DK
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_rtp_receiver LANGUAGES CXX)

# Enable C++17
set_property(TARGET app PROPERTY CXX_STANDARD 17)

# Remove -nostdinc++ to allow STL headers
target_compile_options(app PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-U_GLIBCXX_USE_C99>
)

# Explicitly add C++ include paths from toolchain
target_include_directories(app PRIVATE
    $ENV{ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/arm-zephyr-eabi/include/c++/12.2.0
    $ENV{ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/arm-zephyr-eabi/include/c++/12.2.0/arm-zephyr-eabi
)

# Source files
target_sources(app PRIVATE
    src/main.cpp
    src/net/wifi_manager.cpp
    src/net/rtp_receiver.cpp
    src/cli/shell_commands.cpp
)

# Include directories
target_include_directories(app PRIVATE
    src/
    src/net/
    src/cli/
)

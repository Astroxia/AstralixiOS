cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0057 NEW)

# Check for PICO_SDK_PATH
if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH environment variable is not set. Please set it to the path of your Pico SDK installation.")
endif()

set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Set user home directory
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

# SDK and toolchain versions
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)

# Include Pico VS Code integration if available
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# Set toolchain and import SDK
set(CMAKE_TOOLCHAIN_FILE ${PICO_TOOLCHAIN_FILE})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set board type to Pico 2 W
set(PICO_BOARD pico2_w CACHE STRING "Board type" FORCE)

# Project definition
project(astralixi-os LANGUAGES C CXX ASM)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable with all source files
add_executable(astralixi-os
    astralixi-os.c
    drivers/display.c
    drivers/lcd.c
    drivers/keyboard.c
    drivers/sdcard.c
    drivers/audio.c
    drivers/fat32.c
    drivers/picocalc.c
    drivers/southbridge.c
    drivers/font-8x10.c      
    drivers/font-5x10.c
)

# Generate PIO headers for audio
pico_generate_pio_header(astralixi-os ${CMAKE_CURRENT_LIST_DIR}/drivers/audio.pio)

# Link required libraries
target_link_libraries(astralixi-os
    pico_stdlib
    hardware_spi
    hardware_uart
    hardware_pwm
    hardware_i2c
    hardware_dma
    hardware_gpio
    hardware_adc
    hardware_pio
    hardware_timer
    pico_malloc
    pico_cyw43_arch_lwip_threadsafe_background  # Wi-Fi + lwIP support
    pico_mbedtls                                # TLS/SSL support if needed
)

# Enable stdio over USB and disable UART stdio
pico_enable_stdio_usb(astralixi-os 1)
pico_enable_stdio_uart(astralixi-os 0)

# Add extra outputs (UF2, hex, bin files)
pico_add_extra_outputs(astralixi-os)

# Compiler options
target_compile_options(astralixi-os PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-enum-conversion
)

# Include directories (removed redundant entries)
target_include_directories(astralixi-os PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    drivers/
)

# Ensure MBEDTLS_CONFIG_FILE is set for the build
target_compile_definitions(astralixi-os PRIVATE
    CYW43_USE_SPI
    PICO_CYW43_ARCH_LWIP=1
    LWIP_IPV4=1
    LWIP_DHCP=1
    LWIP_TCP=1
    LWIP_UDP=1
    MBEDTLS_CONFIG_FILE="mbedtls_config.h"
)

# Enable compile commands export for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

# Add preprocessor definitions for WiFi and networking
target_compile_definitions(astralixi-os PRIVATE
    CYW43_USE_SPI
    PICO_CYW43_ARCH_LWIP=1
    LWIP_IPV4=1
    LWIP_DHCP=1
    LWIP_TCP=1
    LWIP_UDP=1
)

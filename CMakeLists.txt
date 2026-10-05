# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Must be set before the SDK is imported
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Copy this file from $PICO_SDK_PATH/external/pico_sdk_import.cmake
include(pico_sdk_import.cmake)

project(radar_ble_bridge C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_executable(radar_ble_bridge
    src/main.cpp
)

# btstack_config.h and the generated GATT header are looked up here
target_include_directories(radar_ble_bridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(radar_ble_bridge PRIVATE
    pico_stdlib
    pico_btstack_ble
    pico_btstack_cyw43
    # Background (interrupt-driven) cyw43/BTstack processing, no lwIP
    pico_cyw43_arch_none
)

# Compiles src/radar_gatt.gatt into radar_gatt.h (ATT database + handle macros)
pico_btstack_make_gatt_header(radar_ble_bridge PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/radar_gatt.gatt"
)

# Logs go to the USB serial port; UART1 is reserved for the radar link
pico_enable_stdio_usb(radar_ble_bridge 1)
pico_enable_stdio_uart(radar_ble_bridge 0)

# Wait up to 3 s for a terminal to open the USB serial port at boot,
# so the startup log lines (BTstack up, advertising) are not lost.
# Boot continues after the timeout even with no terminal attached.
target_compile_definitions(radar_ble_bridge PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=3000
)

pico_add_extra_outputs(radar_ble_bridge)

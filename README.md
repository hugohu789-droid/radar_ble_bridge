# Radar BLE Bridge

A Bluetooth Low Energy bridge for the [desktop radar](../desktop_radar):
a Raspberry Pi Pico 2 W receives radar frames from the STM32 Nucleo over
UART, validates them and forwards them to a phone over a custom GATT
service.

```
Nucleo-F446RE (Zephyr)  --UART 115200-->  Pico 2 W (BTstack)  --BLE GATT-->  Phone (nRF Connect)
   servo + ultrasonic                       CYW43439                         later: Qt radar_host
```

## Status

| Phase | Content | State |
| --- | --- | --- |
| 1 | BTstack bring-up: advertise custom service, heartbeat notify | In progress |
| 2 | Radar data over GATT: UART frames, batching, threshold write, link status | Planned |
| 3 | Robustness and performance: reconnect, UART loss detection, MTU, connection parameters, measurements | Planned |
| 4 | Security: LE Secure Connections pairing, bonding, encrypted writes | Planned |
| 5 | Optional: BLE transport in the Qt host app | Planned |

Problems met and how they were solved are logged in [docs/DEVLOG.md](docs/DEVLOG.md).

## Hardware

| Nucleo-F446RE | | Pico 2 W |
| --- | --- | --- |
| A0 (PA0, UART4 TX) | → | GP5 (pin 7, UART1 RX) |
| A1 (PA1, UART4 RX) | ← | GP4 (pin 6, UART1 TX) |
| GND | — | GND (pin 8) |

Both boards use 3.3 V logic. The Pico is powered from its own USB port,
which also carries its log output (USB serial).

## GATT

Custom 128-bit UUIDs share the base `F5D3xxxx-3515-47D6-879D-3E34DE8F197A`.

| UUID | Name | Properties | Phase |
| --- | --- | --- | --- |
| `F5D30001-…` | Radar Bridge service | | 1 |
| `F5D30002-…` | Radar Data | Notify | 2 |
| `F5D30003-…` | Alarm Threshold | Write | 2 |
| `F5D30004-…` | Link Status | Read, Notify | 2 |
| `F5D30005-…` | Heartbeat counter (uint32 LE, +1 per second) | Read, Notify | 1 |

The device advertises as `RadarBLE` with the service UUID in the
advertising data.

## Build and flash

Built with the Pico SDK 2.3 (not with Zephyr/west). The easiest route on
Windows is the Raspberry Pi Pico VS Code extension:

1. Import Project → this folder, SDK 2.3.0, board Pico 2 W
2. Compile → `build/radar_ble_bridge.uf2`
3. Hold BOOTSEL while plugging in USB, then copy the `.uf2` to the
   `RP2350` drive (or use Run)

Command line, with `PICO_SDK_PATH` and an ARM GNU toolchain set up:

```
cmake -G Ninja -B build
cmake --build build
```

## Project layout

```
├── CMakeLists.txt
├── pico_sdk_import.cmake   # copied from the Pico SDK
├── docs/DEVLOG.md          # problems and fixes per phase
└── src/
    ├── main.cpp            # BTstack setup, GATT callbacks, heartbeat
    ├── radar_gatt.gatt     # GATT database (compiled to radar_gatt.h)
    └── btstack_config.h    # BTstack feature and buffer configuration
```

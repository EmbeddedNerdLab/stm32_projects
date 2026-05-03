# STM32F446RE FreeRTOS Multi-Sensor Project

A complete embedded system built around the STM32F446RE (Nucleo-64 board), combining a custom bootloader, FreeRTOS application firmware, and a Qt6 desktop monitoring app.

---

## Overview

| Component | Description |
|---|---|
| **Bootloader** | Custom UART bootloader with AES-128-CTR encrypted firmware update |
| **App Firmware** | FreeRTOS application reading 5 sensors, streaming over USB CDC |
| **Desktop App** | Qt6 dashboard — live charts, sensor cards, SQLite logging, firmware update UI |

---

## Hardware

**Board:** STM32F446RE Nucleo-64

| Peripheral | Interface | Pin(s) |
|---|---|---|
| MPU6050 (IMU — accel/gyro) | I2C1 | PB8 SCL, PB9 SDA |
| BME280 (temp/humidity/pressure) | I2C1 | PB8 SCL, PB9 SDA |
| BH1750 (ambient light) | I2C1 | PB8 SCL, PB9 SDA |
| HC-SR04 (ultrasonic distance) | GPIO | trigger/echo |
| Passive buzzer | TIM3 CH1 PWM | PB4 |
| USB CDC (sensor stream) | USB FS | PA11/PA12 |
| UART2 (console/bootloader) | USART2 | PA2 TX, PA3 RX |
| USER button (DFU override) | GPIO | PC13 |
| LD2 LED | GPIO | PA5 |

---

## Repository Structure

```
stm32_projects/
├── firmware/
│   ├── app/                  # FreeRTOS application firmware
│   │   ├── Core/             # main, HAL MSP, interrupts, FreeRTOS tasks
│   │   ├── Drivers/          # Sensor drivers (MPU6050, BME280, BH1750, HC-SR04, buzzer)
│   │   ├── USB/              # USB CDC application layer
│   │   └── CMakeLists.txt
│   ├── bootloader/           # Custom UART bootloader
│   │   ├── Core/             # main, boot protocol, AES-128
│   │   └── CMakeLists.txt
│   ├── libs/                 # Git submodules
│   │   ├── FreeRTOS-Kernel
│   │   ├── stm32f4xx-hal-driver
│   │   ├── cmsis-device-f4
│   │   ├── CMSIS_5
│   │   └── stm32_mw_usb_device
│   ├── cmake/                # ARM cross-compiler toolchain file
│   └── tools/
│       └── fw_encrypt.py     # Offline firmware encryption helper
└── software/                 # Qt6 desktop application (Windows)
    ├── mainwindow.*          # Dashboard UI
    ├── serialworker.*        # Serial port thread
    ├── flashworker.*         # Firmware update worker (AES-128-CTR, CRC32)
    ├── flashdialog.*         # Firmware update dialog
    ├── widgets.*             # Custom gauge widgets
    ├── database.*            # SQLite logging
    ├── dataparser.*          # UART line parser
    └── style.qss             # Dark theme stylesheet
```

---

## Firmware Architecture

### Bootloader (`firmware/bootloader/`)

- Lives at flash sector 0–1 (`0x08000000`, 32 KB)
- On power-up: checks RTC backup register (`BKP1R`) for update magic **or** PC13 (USER button) held low
- If neither: jumps to application at `0x08010000`
- LED patterns: fast blink = waiting for host, solid = flashing, 3 blinks = success
- **Update protocol over UART2 at 115200 baud:**
  1. Bootloader sends `ACK`
  2. Host sends `START` frame: nonce (12 B) + firmware size (4 B) + CRC32 (4 B) + CRC16 frame check
  3. Bootloader erases sectors 4–7, sends second `ACK`
  4. Host sends 256-byte `DATA` chunks, each with CRC16
  5. Host sends `END`; bootloader verifies full CRC32, resets into new firmware
- Firmware encrypted with **AES-128-CTR** — key is compiled into both bootloader and desktop app
- **DFU recovery:** hold USER button (PC13) and press RESET to force bootloader mode even with corrupted firmware

### Application Firmware (`firmware/app/`)

**FreeRTOS tasks:**

| Task | Priority | Period | Function |
|---|---|---|---|
| `vSensorReadTask` | 1 | 500 ms | Reads all sensors, streams over USB CDC, controls buzzer events |
| `vBuzzerTask` | 3 | event-driven | Activates buzzer (440/880 Hz) when light level exceeds threshold |
| `vCommandTask` | 2 | 5 ms poll | Listens for UART command `0x05` → triggers reboot to bootloader |

**Sensor output (USB CDC, 500 ms interval):**
```
IMU  AX=   234 AY=  -512 AZ= 16200  GX=    12 GY=    -8 GZ=     3
ENV  T=24.53 C  H=45.8%  P=1013 hPa  LUX=320  DIST=123 cm
```

---

## Desktop Application (`software/`)

Qt6 app for Windows with a dark dashboard:

- **Live charts:** scrolling accelerometer and gyroscope charts (last 120 points), configurable signal selection
- **Sensor cards:** real-time values with visual gauge widgets (thermometer, humidity drop, lux lamp, pressure, distance, orientation)
- **Complementary filter:** roll/pitch/yaw computed from IMU data (α = 0.96)
- **CSV logging:** one-click logging to `.csv` file
- **SQLite logging:** every record stored to `stm32_log.db`
- **Firmware update:** two-step flow — *Bootloader* button sends command over open serial connection, *Flash* button opens update dialog to select `.bin` and upload

---

## Building

### Prerequisites

- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (arm-none-eabi, tested with 14.2)
- CMake ≥ 3.20 + Ninja
- Qt 6.x with modules: **Charts**, **SerialPort**, **Sql** (MSVC 2022 64-bit or MinGW 64-bit)

### Clone with submodules

```bash
git clone --recurse-submodules https://github.com/<your-username>/stm32_projects.git
```

If already cloned:
```bash
git submodule update --init --recursive
```

### Build bootloader

```bash
cmake -B firmware/bootloader/build -S firmware/bootloader \
      -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-none-eabi-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja -C firmware/bootloader/build
```

Output: `firmware/bootloader/build/stm32_bootloader.bin`

### Build application firmware

```bash
cmake -B firmware/app/build -S firmware/app \
      -DCMAKE_TOOLCHAIN_FILE=$(pwd)/firmware/cmake/arm-none-eabi-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Debug -G Ninja
ninja -C firmware/app/build
```

Output: `firmware/app/build/stm32_freertos_multisensor.bin`

### Build Qt desktop app

```bash
cmake -B software/build -S software \
      -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/msvc2022_64 \
      -DCMAKE_BUILD_TYPE=Release
cmake --build software/build --config Release
```

---

## Flashing

> Requires [OpenOCD](https://openocd.org/) and an ST-Link (built into the Nucleo board).

**Flash bootloader first (only needed once):**
```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program firmware/bootloader/build/stm32_bootloader.bin 0x08000000 verify reset exit"
```

**Flash application firmware:**
```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program firmware/app/build/stm32_freertos_multisensor.bin 0x08010000 verify reset exit"
```

**OTA update via desktop app:**
1. Connect board over USB/UART
2. Click **Bootloader** button — board reboots, LED fast-blinks
3. Click **Flash** button — select `.bin`, click Start Update

**Recovery (corrupted firmware):** hold USER button (PC13) and press RESET → board enters bootloader mode regardless of app state.

---

## Memory Layout

| Region | Address | Size | Contents |
|---|---|---|---|
| Bootloader | `0x08000000` | 32 KB (sectors 0–1) | Bootloader code |
| *(reserved)* | `0x08008000` | 96 KB (sectors 2–3) | Unused |
| Application | `0x08010000` | 448 KB (sectors 4–7) | FreeRTOS app |
| SRAM | `0x20000000` | 128 KB | Stack, heap, FreeRTOS |

---

## License

This project is released under the MIT License.

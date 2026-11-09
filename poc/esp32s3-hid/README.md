# ESP32-S3 DualSense HID POC-1

This is an isolated ESP-IDF project for the first migration proof of concept. It does not depend on the existing Pico SDK firmware.

## Scope

- Target: ESP32-S3 USB OTG device
- USB identity: Sony `054C:0CE6`
- Interface: one HID gamepad interface
- Report descriptor: copied from the existing DualSense descriptor and checked to 337 bytes
- Input: 63-byte report body, sent with Report ID `0x01` every 4 ms
- Output: received HID reports are logged over the ESP-IDF console

Bluetooth, UART, UAC1 audio, DualSense Edge, and the debug host interface are intentionally out of scope for POC-1.

## Build

The repository includes ESP-IDF v6.1-compatible descriptor setup. On Windows, use an ESP-IDF CMD/PowerShell terminal rather than MSYS Bash:

```bat
set IDF_PATH=E:\ESP-IDF\esp\v6.1\esp-idf
call %IDF_PATH%\export.bat
cd /d E:\Code\git_projects\esp\DS5DongleESP\poc\esp32s3-hid
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

The project dependency is `espressif/esp_tinyusb`, resolved by ESP-IDF Component Manager. `uv` can provide the required Python interpreter, but ESP-IDF's `export.bat` must also activate its toolchain and Python environment. The current MSYS shell found the ESP-IDF source tree but could not activate it because `idf6.1_py3.11_env` is not installed.
